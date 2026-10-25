//------------------------------------------------------------------------------------------------
// CGS Portal - live, client-local, two-way camera surfaces for GUN portals.
//
// Blue's ViewPlane renders from its owner's Orange opening.
// Orange's ViewPlane renders from its owner's Blue opening.
// Server owns shooting and teleportation; NOTHING here teleports anyone.
//
// The successful Workbench prototype proved:
// 1) ScriptCamera plus RenderTargetWidget works in the same world.
// 2) RTTextureWidget.SetRenderTarget(portal entity) needs a runtime XOB
//    $remap from the ViewPlane's ORIGINAL SOURCE material name.
// 3) MatPBRBasic with BCRMap + EmissiveMap "$rendertarget" works.
// 4) HDR must be copied from the local player's current camera, every frame.
//
// Multiplayer: the owner's ID is now RplProp on CGS_PortalComponent. Each
// client builds its OWN pair lookup from its replicated portals. No camera
// textures and no player camera data are transmitted over the network.
//
// Enfusion only provides a finite number of camera slots. Limit each client
// to TWO active in-world live views, choosing the nearest eligible portals.
// Each camera defaults to 12 FPS and half resolution. Others remain visible
// as frames but do not consume a live render until nearer to the viewer.
//
// Future work: viewer-relative portal parallax and recursive portal clipping.
//------------------------------------------------------------------------------------------------

[ComponentEditorProps(category: "CGS Portal", description: "Client-side live picture-in-picture visual for personal gun portals (pair by replicated owner ID).")]
class CGS_PortalPIPLiveComponentClass : ScriptComponentClass
{
}

class CGS_PortalPIPLiveComponent : ScriptComponent
{
	[Attribute("1", UIWidgets.CheckBox, "Enable live portal surface; does not affect teleportation.")]
	protected bool m_bEnabled;

	[Attribute("{6AB25DDA4F028000}UI/Layouts/CGS_PortalPIP_Test.layout", UIWidgets.ResourceNamePicker, "Proven RTTexture0 / RenderTarget0 rendering layout", "layout")]
	protected ResourceName m_sLayout;

	[Attribute("12", UIWidgets.EditBox, "Max live camera render FPS for this portal (1-30).")]
	protected int m_iMaxFPS;

	[Attribute("0.5", UIWidgets.EditBox, "Local render resolution scale (0.2-1.0).")]
	protected float m_fRenderScale;

	[Attribute("75", UIWidgets.EditBox, "Remote camera vertical field of view, degrees.")]
	protected float m_fFOV;

	[Attribute("0.25", UIWidgets.EditBox, "Distance in front of partner portal for remote camera (meters).")]
	protected float m_fCameraOutset;

	[Attribute("120", UIWidgets.EditBox, "Maximum camera distance to the portal for local live rendering; limits GPU cost.")]
	protected float m_fLocalRenderDistance;

	[Attribute("1", UIWidgets.CheckBox, "Copy the local player's actual HDR exposure into the remote camera each frame.")]
	protected bool m_bMatchPlayerHDR;

	[Attribute("1.0", UIWidgets.EditBox, "Remote camera HDR multiplier, 1 = matched; 1.5-2 if needed.")]
	protected float m_fHDRMultiplier;

	[Attribute("0", UIWidgets.CheckBox, "Log one-time setup/teardown and pair diagnostics (no frame spam).")]
	protected bool m_bDebug;

	// Camera 26/27 are reserved for the TWO nearest visible gun portals
	// on THIS client. Workbench static PIP tests use slots 20/21.
	protected static const int CAMERA_SLOT_BLUE_NEAR = 26;
	protected static const int CAMERA_SLOT_ORANGE_NEAR = 27;
	protected static const int SELECTION_INTERVAL_MS = 450;
	protected static const int WARMUP_MS = 1500;

	protected static ref array<CGS_PortalPIPLiveComponent> s_aPortals = {};
	protected static CGS_PortalPIPLiveComponent s_pBestFirst;
	protected static CGS_PortalPIPLiveComponent s_pBestSecond;
	protected static int s_iLastSelectMs = -10000;

	protected CGS_PortalComponent m_pPortal;
	protected CGS_PortalPIPLiveComponent m_pPartner;
	protected ref Widget m_wRoot;
	protected RTTextureWidget m_wTexture;
	protected RenderTargetWidget m_wView;
	protected ScriptCamera m_pCamera;
	protected bool m_bRendering;
	protected bool m_bRenderTargetBound;
	protected int m_iCameraSlot = -1;
	protected int m_iSelectedSinceMs = -1;
	protected bool m_bNotifiedLive;
	protected bool m_bLoggedMaterialFailure;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		SetEventMask(owner, EntityEvent.INIT | EntityEvent.FRAME);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		m_pPortal = CGS_PortalComponent.Cast(owner.FindComponent(CGS_PortalComponent));
		if (!m_pPortal)
			return;

		// Register on client too: server manager's static map does NOT
		// automatically replicate. The owner ID on each portal does.
		if (!s_aPortals.Contains(this))
			s_aPortals.Insert(this);
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasLocalRenderer()
	{
		// Dedicated server gets no widgets and no ScriptCamera.
		return GetGame() && GetGame().GetPlayerController() && GetGame().GetWorkspace();
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsLiveEntity()
	{
		IEntity owner = GetOwner();
		return owner && !owner.IsDeleted() && owner.GetWorld();
	}

	//------------------------------------------------------------------------------------------------
	protected CGS_PortalPIPLiveComponent FindPartner()
	{
		if (!IsLiveEntity() || !m_pPortal)
			return null;

		int ownerId = m_pPortal.GetPortalOwnerID();
		// Gun portals are assigned ID on the SERVER just after spawn.
		// 0 on a client means the replicated identity is not ready.
		// Never accidentally pair the gun with world/test portals.
		if (ownerId <= 0)
			return null;

		BaseWorld world = GetOwner().GetWorld();
		int color = m_pPortal.GetPortalID();

		foreach (CGS_PortalPIPLiveComponent candidate : s_aPortals)
		{
			if (!candidate || candidate == this || !candidate.IsLiveEntity())
				continue;
			if (!candidate.m_bEnabled || !candidate.m_pPortal)
				continue;
			if (candidate.GetOwner().GetWorld() != world)
				continue;
			if (candidate.m_pPortal.GetPortalID() == color)
				continue;
			if (candidate.m_pPortal.GetPortalOwnerID() != ownerId)
				continue;

			return candidate;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	// Run once every ~0.45 seconds PER CLIENT, not on every portal each
	// frame. Two closest eligible live portal faces get camera slots 26/27.
	// This avoids creating dozens of camera/world render passes when many
	// players own portal guns on a 128-player server.
	protected static void RefreshLocalRenderSelection()
	{
		if (!GetGame() || !GetGame().GetWorld() || !GetGame().GetPlayerController())
			return;

		int now = GetGame().GetWorld().GetWorldTime();
		if (now >= s_iLastSelectMs && now - s_iLastSelectMs < SELECTION_INTERVAL_MS)
			return;
		s_iLastSelectMs = now;

		vector viewer[4];
		GetGame().GetWorld().GetCurrentCamera(viewer);
		vector viewerPosition = viewer[3];

		CGS_PortalPIPLiveComponent first;
		CGS_PortalPIPLiveComponent second;
		float firstDistanceSq = 1000000000.0;
		float secondDistanceSq = 1000000000.0;

		foreach (CGS_PortalPIPLiveComponent candidate : s_aPortals)
		{
			if (!candidate || !candidate.m_bEnabled || !candidate.IsLiveEntity())
				continue;
			if (candidate.GetOwner().GetWorld() != GetGame().GetWorld())
				continue;
			if (!candidate.FindPartner())
				continue;

			vector portalWorld[4];
			candidate.GetOwner().GetWorldTransform(portalWorld);
			float distanceSq = vector.DistanceSq(viewerPosition, portalWorld[3]);
			float limit = candidate.m_fLocalRenderDistance;
			if (limit <= 0)
				limit = 120;
			if (distanceSq > limit * limit)
				continue;

			if (distanceSq < firstDistanceSq)
			{
				second = first;
				secondDistanceSq = firstDistanceSq;
				first = candidate;
				firstDistanceSq = distanceSq;
			}
			else if (distanceSq < secondDistanceSq)
			{
				second = candidate;
				secondDistanceSq = distanceSq;
			}
		}

		// Stop prior pass(es) BEFORE reusing their camera slot(s).
		// Swapping the rank of two portals also changes their camera
		// slots, so each portal changing rank must release its old pass.
		if (s_pBestFirst && s_pBestFirst != first)
			s_pBestFirst.StopRender();

		if (s_pBestSecond && s_pBestSecond != second)
			s_pBestSecond.StopRender();

		s_pBestFirst = first;
		s_pBestSecond = second;
	}

	//------------------------------------------------------------------------------------------------
	protected int GetAssignedCameraSlot()
	{
		if (s_pBestFirst == this)
			return CAMERA_SLOT_BLUE_NEAR;
		if (s_pBestSecond == this)
			return CAMERA_SLOT_ORANGE_NEAR;
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	// Identical proven material remapping to the successful Blue PIP test.
	// IMPORTANT: remap only the center ViewPlane source slot. Keep metal
	// frame and colored glowing ring on their own materials.
	protected bool ApplyLiveScreenMaterial()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return false;

		VObject mesh = owner.GetVObject();
		if (!mesh)
			return false;

		string expectedSource = "CGS_Portal_BLUE_Screen_PLACEHOLDER";
		if (m_pPortal.GetPortalID() == 1)
			expectedSource = "CGS_Portal_ORANGE_Screen_PLACEHOLDER";

		ResourceName screenMaterial = "{6AB25DDA4F021001}Common/Models/Data/CGS_Portal_PIP_Screen_TEST.emat";
		Resource material = Resource.Load(screenMaterial);
		if (!material || !material.IsValid())
		{
			if (!m_bLoggedMaterialFailure)
			{
				m_bLoggedMaterialFailure = true;
				Print("[CGS GUN PIP] ERROR: Render-target screen material failed to load.", LogLevel.ERROR);
			}
			return false;
		}

		string materials[256];
		int count = mesh.GetMaterials(materials);
		int scanCount = count;
		if (scanCount > 256)
			scanCount = 256;

		bool hasSource;
		for (int i = 0; i < scanCount; i++)
		{
			if (materials[i] == expectedSource)
			{
				hasSource = true;
				break;
			}
		}

		// The source names were confirmed via Workbench logs for Blue.
		// Orange uses the corresponding source name from its XOB.meta.
		if (!hasSource && m_bDebug)
			PrintFormat("[CGS GUN PIP] ViewPlane source not listed: %1; attempting exact source remap.", expectedSource);

		string remap = string.Format("$remap '%1' '%2';", expectedSource, screenMaterial);
		owner.SetObject(mesh, remap);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateRemoteCamera()
	{
		if (!m_pPartner || !m_pPartner.IsLiveEntity() || !m_pPartner.m_pPortal)
			return;

		vector right;
		vector up;
		vector normal;
		m_pPartner.m_pPortal.GetPortalBasis(right, up, normal);

		// Respect actual gun placement on wall, floor, ceiling or slope.
		// DO NOT apply the old static test's horizontal camera fallback:
		// it would make floor and ceiling portal views point the wrong way.
		vector partnerWorld[4];
		m_pPartner.GetOwner().GetWorldTransform(partnerWorld);

		float offset = Math.Clamp(m_fCameraOutset, 0.15, 1.0);
		vector transform[4];
		transform[0] = right;
		transform[1] = up;
		transform[2] = normal;
		transform[3] = partnerWorld[3] + normal * offset;

		if (m_pCamera)
		{
			m_pCamera.SetWorldTransform(transform);
			m_pCamera.FreeFly = false;
		}

		GetOwner().GetWorld().SetCameraEx(m_iCameraSlot, transform);
	}

	//------------------------------------------------------------------------------------------------
	protected void SyncExposure()
	{
		if (!m_bMatchPlayerHDR || m_iCameraSlot < 0 || !HasLocalRenderer())
			return;

		BaseWorld world = GetOwner().GetWorld();
		int mainIndex = 0;
		CameraManager manager = GetGame().GetCameraManager();
		if (manager)
		{
			CameraBase camera = manager.CurrentCamera();
			if (camera)
				mainIndex = camera.GetCameraIndex();
		}

		if (mainIndex == m_iCameraSlot)
			mainIndex = 0;

		float exposure = world.GetCameraHDRBrightness(mainIndex);
		if (exposure <= 0)
			return;

		world.SetCameraHDRBrightness(m_iCameraSlot, exposure * Math.Clamp(m_fHDRMultiplier, 0.25, 8.0));
	}

	//------------------------------------------------------------------------------------------------
	protected bool StartRender(int cameraSlot, CGS_PortalPIPLiveComponent partner)
	{
		if (!HasLocalRenderer() || !partner || !IsLiveEntity())
			return false;

		// Use the proven material recipe BEFORE binding $rendertarget.
		if (!ApplyLiveScreenMaterial())
			return false;

		BaseWorld world = GetOwner().GetWorld();
		m_pPartner = partner;
		m_iCameraSlot = cameraSlot;

		m_pCamera = ScriptCamera.Cast(GetGame().SpawnEntity(ScriptCamera, world));
		if (!m_pCamera)
		{
			m_iCameraSlot = -1;
			return false;
		}

		float fov = Math.Clamp(m_fFOV, 25.0, 110.0);
		m_pCamera.Index = cameraSlot;
		m_pCamera.FreeFly = false;
		m_pCamera.FOV = fov;
		m_pCamera.NearPlane = 0.05;
		m_pCamera.FarPlane = 1200.0;

		UpdateRemoteCamera();
		world.SetCameraVerticalFOV(cameraSlot, fov);
		world.SetCameraNearPlane(cameraSlot, 0.05);
		world.SetCameraFarPlane(cameraSlot, 1200.0);
		world.SetCameraType(cameraSlot, CameraType.PERSPECTIVE);
		world.SetCameraLensFlareSet(cameraSlot, CameraLensFlareSetType.FirstPerson, string.Empty);
		SyncExposure();

		m_wRoot = GetGame().GetWorkspace().CreateWidgets(m_sLayout);
		if (!m_wRoot)
		{
			StopRender();
			return false;
		}

		m_wTexture = RTTextureWidget.Cast(m_wRoot.FindAnyWidget("RTTexture0"));
		m_wView = RenderTargetWidget.Cast(m_wRoot.FindAnyWidget("RenderTarget0"));
		if (!m_wTexture || !m_wView)
		{
			StopRender();
			return false;
		}

		m_wView.SetWorld(world, cameraSlot);
		float resolution = Math.Clamp(m_fRenderScale, 0.2, 1.0);
		m_wView.SetResolutionScale(resolution, resolution);
		m_wView.SetVisible(true);

		int fps = m_iMaxFPS;
		if (fps < 1)
			fps = 1;
		if (fps > 30)
			fps = 30;
		m_wView.SetMaxFPS(fps);
		m_wView.SetRefresh(1, 0);
		m_wTexture.SetRenderTarget(GetOwner());
		m_bRenderTargetBound = true;
		m_bRendering = true;

		if (m_bDebug)
			PrintFormat("[CGS GUN PIP] LIVE owner=%1 color=%2 remoteColor=%3 slot=%4 fps=%5 scale=%6", m_pPortal.GetPortalOwnerID(), m_pPortal.GetPortalID(), m_pPartner.m_pPortal.GetPortalID(), cameraSlot, fps, resolution);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void StopRender()
	{
		// A camera slot must be released BEFORE another visible portal
		// tries to use it. Stop running UI/render targets on this client.
		if (m_bRenderTargetBound && m_wTexture && GetOwner() && !GetOwner().IsDeleted())
			m_wTexture.RemoveRenderTarget(GetOwner());

		m_bRenderTargetBound = false;

		if (m_wView && m_iCameraSlot >= 0)
			m_wView.SetWorld(null, m_iCameraSlot);

		if (m_wRoot)
			m_wRoot.RemoveFromHierarchy();

		m_wRoot = null;
		m_wView = null;
		m_wTexture = null;

		if (m_pCamera)
		{
			delete m_pCamera;
			m_pCamera = null;
		}

		if (m_bRendering && m_bDebug)
			PrintFormat("[CGS GUN PIP] STOP owner=%1 color=%2 released slot=%3", m_pPortal.GetPortalOwnerID(), m_pPortal.GetPortalID(), m_iCameraSlot);

		m_pPartner = null;
		m_iCameraSlot = -1;
		m_iSelectedSinceMs = -1;
		m_bRendering = false;
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (!m_bEnabled || !m_pPortal || !HasLocalRenderer())
			return;

		RefreshLocalRenderSelection();

		int requiredSlot = GetAssignedCameraSlot();
		CGS_PortalPIPLiveComponent partner = FindPartner();
		if (requiredSlot < 0 || !partner)
		{
			if (m_bRendering)
				StopRender();
			else
				m_iSelectedSinceMs = -1;
			return;
		}

		if (m_bRendering)
		{
			// Shot a new partner portal, or rendering budget rotated?
			// Discard stale camera and rebind to the latest owner pair.
			if (m_iCameraSlot != requiredSlot || m_pPartner != partner)
			{
				StopRender();
				return;
			}

			UpdateRemoteCamera();
			SyncExposure();
			return;
		}

		int now = GetGame().GetWorld().GetWorldTime();
		if (m_iSelectedSinceMs < 0)
		{
			m_iSelectedSinceMs = now;
			return;
		}

		if (now - m_iSelectedSinceMs < WARMUP_MS)
			return;

		if (!StartRender(requiredSlot, partner))
			m_iSelectedSinceMs = now; // Retry only after another warmup.
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		StopRender();

		if (s_pBestFirst == this)
			s_pBestFirst = null;
		if (s_pBestSecond == this)
			s_pBestSecond = null;

		s_aPortals.RemoveItem(this);
		s_iLastSelectMs = -10000;
		super.OnDelete(owner);
	}
}
