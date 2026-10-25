//------------------------------------------------------------------------------------------------
// CGS Portal — LOCAL preview-only picture-in-picture proof of concept.
//
// Uses the SAME engine pipeline as Reforger's own PIP weapon optics:
//    RenderTargetWidget.SetWorld(world, cameraIndex)
//    RTTextureWidget.SetRenderTarget(portalEntity)
//    BCRMap "$rendertarget" on the portal screen material.
//
// Add ONLY to the new *_PIP_Test.et prefabs. Existing gameplay portal
// prefabs, controls, ownership and teleport code remain untouched.
//
// STAGE 1: enable Blue, disable Orange. Confirm one live camera feed first.
// STAGE 2: enable Orange after Stage 1 works. Cameras can recursively see
// each other's screens; this proof of concept does not solve recursion.
//
// This is a ONE-PAIR, LOCAL visual test (camera slot 20 for Blue, 21 for
// Orange). Owner IDs are not replicated to clients in the current project;
// do not claim personal pairs / long-distance multiplayer work yet.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "CGS Portal", description: "EXPERIMENTAL local portal PiP render-to-mesh test; not part of the stable gun portals.")]
class CGS_PortalPIPTestComponentClass : ScriptComponentClass
{
}

class CGS_PortalPIPTestComponent : ScriptComponent
{
	[Attribute("1", UIWidgets.CheckBox, "Experimental portal camera feed active. Enable BLUE first; keep ORANGE off until Blue works.")]
	protected bool m_bPreviewEnabled;

	[Attribute("20", UIWidgets.EditBox, "LOCAL camera index. Use 20 for Blue, 21 for Orange; never reuse a camera slot in the same client.")]
	protected int m_iCameraIndex;

	[Attribute("{6AB25DDA4F028000}UI/Layouts/CGS_PortalPIP_Test.layout", UIWidgets.ResourceNamePicker, "Portal-specific PIP test layout: 512 x 512 RTTexture0 + RenderTarget0", "layout")]
	protected ResourceName m_sPIPLayout;

	[Attribute("12", UIWidgets.EditBox, "Target PIP FPS (first test: 12)")]
	protected int m_iMaxFPS;

	[Attribute("0.5", UIWidgets.EditBox, "Render resolution scale (0.5 = half)")]
	protected float m_fResolutionScale;

	[Attribute("75", UIWidgets.EditBox, "Remote camera vertical FOV in degrees")]
	protected float m_fCameraFOV;

	[Attribute("0.25", UIWidgets.EditBox, "Distance in front of partner portal (meters)")]
	protected float m_fCameraOutset;

	[Attribute("1", UIWidgets.CheckBox, "Print one-time PIP state logs")]
	protected bool m_bDebug;

	[Attribute("1", UIWidgets.CheckBox, "Show temporary 256px camera preview in local HUD. If HUD works but model is blank, check XOB screen material.")]
	protected bool m_bDebugHUD;

	[Attribute("{6AB25DDA4F038010}UI/Layouts/CGS_PortalPIP_DebugHUD.layout", UIWidgets.ResourceNamePicker, "Temporary independent HUD image to check render target separately from the portal material", "layout")]
	protected ResourceName m_sDebugHUDLayout;

	[Attribute("1", UIWidgets.CheckBox, "Only for first PIP test: if partner normal points straight up/down, point camera horizontally to see scenery.")]
	protected bool m_bUseHorizontalTestView;

	protected static ref array<CGS_PortalPIPTestComponent> s_aTests = {};

	protected CGS_PortalComponent m_pPortal;
	protected CGS_PortalComponent m_pPartner;

	protected ref Widget m_wUIRoot;
	protected RTTextureWidget m_wRTTexture;
	protected RenderTargetWidget m_wView;
	protected ScriptCamera m_pFeedCamera;
	protected ref Widget m_wDebugHUD;
	protected ImageWidget m_wDebugHUDImage;
	protected bool m_bLoggedNoPartner;
	protected bool m_bInitialized;
	protected bool m_bStartScheduled;
	protected float m_fDiagnosticClock;
	protected bool m_bPrintedCameraReadback;

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
		{
			Print("[CGS PORTAL PIP] Missing CGS_PortalComponent on test prefab.");
			return;
		}

		// This registry is per running client process, NOT network state.
		if (!s_aTests.Contains(this))
			s_aTests.Insert(this);

		if (m_bDebug)
			PrintFormat("[CGS PORTAL PIP] Registered test portal color=%1 enable=%2 camera=%3", m_pPortal.GetPortalID(), m_bPreviewEnabled, m_iCameraIndex);
	}

	//------------------------------------------------------------------------------------------------
	// This static list intentionally supports a SINGLE world-placed test
	// pair, not arbitrary per-player portal pairs on a busy multiplayer map.
	protected CGS_PortalComponent FindOppositeTest()
	{
		if (!m_pPortal)
			return null;

		foreach (CGS_PortalPIPTestComponent candidate : s_aTests)
		{
			if (!candidate || candidate == this || !candidate.GetOwner())
				continue;

			CGS_PortalComponent other = candidate.m_pPortal;
			if (!other || other.GetPortalID() == m_pPortal.GetPortalID())
				continue;

			// In the first static-world test, both owner IDs are 0.
			if (other.GetPortalOwnerID() != m_pPortal.GetPortalOwnerID())
				continue;

			return other;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasLocalRenderer()
	{
		// Dedicated servers must not create UI widgets or view cameras.
		return GetGame() && GetGame().GetPlayerController() && GetGame().GetWorkspace();
	}

	//------------------------------------------------------------------------------------------------
	protected bool SetupPreview()
	{
		if (!HasLocalRenderer() || !m_pPartner || m_bInitialized)
			return false;

		if (m_iCameraIndex < 1 || m_iCameraIndex > 31)
		{
			PrintFormat("[CGS PORTAL PIP] ERROR: Invalid camera slot %1 (valid 1-31)", m_iCameraIndex);
			return false;
		}

		BaseWorld world = GetOwner().GetWorld();
		if (!world)
			return false;

		// FIX: A render target can bind successfully even if it is showing
		// EMPTY pixels. The original prototype configured a world camera
		// slot and called SetCameraEx, but never spawned a ScriptCamera.
		//
		// Both stock Reforger PIP optics and the user-provided, formerly
		// working SVC CCTV mod create a camera ENTITY, set its index,
		// and keep its transform up-to-date. Do that BEFORE the layout.
		m_pFeedCamera = ScriptCamera.Cast(GetGame().SpawnEntity(ScriptCamera, world));
		if (!m_pFeedCamera)
		{
			Print("[CGS PORTAL PIP] ERROR: Could not spawn ScriptCamera for the remote view.");
			return false;
		}

		float fov = Math.Clamp(m_fCameraFOV, 25.0, 110.0);
		m_pFeedCamera.Index = m_iCameraIndex;
		m_pFeedCamera.FreeFly = false;
		m_pFeedCamera.FOV = fov;
		m_pFeedCamera.NearPlane = 0.05;
		m_pFeedCamera.FarPlane = 1200.0;

		// Set the camera transform BEFORE allocating any render target.
		// ScriptCamera.EOnPostFrame also re-applies its transform to
		// this world camera slot. Force FreeFly=false every frame.
		UpdatePartnerCamera();

		// Script-created cameras do not automatically apply their
		// index/FOV/clip values like an authored camera prefab does.
		world.SetCameraVerticalFOV(m_iCameraIndex, fov);
		world.SetCameraNearPlane(m_iCameraIndex, 0.05);
		world.SetCameraFarPlane(m_iCameraIndex, 1200.0);
		world.SetCameraType(m_iCameraIndex, CameraType.PERSPECTIVE);
		world.SetCameraLensFlareSet(m_iCameraIndex, CameraLensFlareSetType.FirstPerson, string.Empty);

		// Use our own known 512px layout, not a potentially masked
		// stock rifle-optics layout.
		m_wUIRoot = GetGame().GetWorkspace().CreateWidgets(m_sPIPLayout);
		if (!m_wUIRoot)
		{
			PrintFormat("[CGS PORTAL PIP] ERROR: Failed to create layout %1", m_sPIPLayout);
			DestroyPreview();
			return false;
		}

		m_wRTTexture = RTTextureWidget.Cast(m_wUIRoot.FindAnyWidget("RTTexture0"));
		m_wView = RenderTargetWidget.Cast(m_wUIRoot.FindAnyWidget("RenderTarget0"));
		if (!m_wRTTexture || !m_wView)
		{
			Print("[CGS PORTAL PIP] ERROR: Test layout is missing RTTexture0 or RenderTarget0");
			DestroyPreview();
			return false;
		}

		// Preserve the known working CCTV/weapon scope order:
		// camera -> widget layout -> SetWorld -> scale -> SetRenderTarget.
		m_wView.SetWorld(world, m_iCameraIndex);

		float scale = Math.Clamp(m_fResolutionScale, 0.2, 1.0);
		m_wView.SetResolutionScale(scale, scale);
		m_wView.SetVisible(true);

		int fps = m_iMaxFPS;
		if (fps < 1)
			fps = 1;
		if (fps > 30)
			fps = 30;
		m_wView.SetMaxFPS(fps);
		m_wView.SetRefresh(1, 0);

		// Material on ONLY the new frame's view plane contains:
		//    BCRMap "$rendertarget"
		m_wRTTexture.SetRenderTarget(GetOwner());

		// The HUD preview samples the SAME RTTextureWidget through a
		// standard ImageWidget API. It bypasses the model's $rendertarget
		// material. This makes camera/layout failures distinguishable from
		// incorrect material assignments or invisible ViewPlane geometry.
		if (m_bDebugHUD)
		{
			m_wDebugHUD = GetGame().GetWorkspace().CreateWidgets(m_sDebugHUDLayout);
			if (m_wDebugHUD)
			{
				m_wDebugHUDImage = ImageWidget.Cast(m_wDebugHUD.FindAnyWidget("PIPDebugImage"));
				if (m_wDebugHUDImage)
				{
					m_wDebugHUDImage.SetImageTexture(0, m_wRTTexture);
					Print("[CGS PORTAL PIP HUD] Local preview created. Top left image = camera feed independent of mesh. If this works but portal stays blank, check the XOB screen material.");
				}
				else
				{
					Print("[CGS PORTAL PIP HUD] WARNING: PIPDebugImage not found in diagnostic HUD layout.");
				}
			}
			else
			{
				Print("[CGS PORTAL PIP HUD] WARNING: Could not load diagnostic HUD layout.");
			}
		}

		m_bInitialized = true;

		if (m_bDebug)
		{
			vector lens[4];
			m_pFeedCamera.GetTransform(lens);
			PrintFormat("[CGS PORTAL PIP CAMERA] ScriptCamera CREATED slot=%1 origin=%2 forward=%3 partnerPosition=%4", m_iCameraIndex, lens[3], lens[2], m_pPartner.GetOwner().GetOrigin());

			vector remoteForward = lens[2];
			if (Math.AbsFloat(remoteForward[1]) >= 0.85)
				Print("[CGS PORTAL PIP CAMERA] NOTE: Camera is vertical; rotate remote test frame or enable horizontal preview.");
			else
				Print("[CGS PORTAL PIP CAMERA] Camera aimed toward scenery (not vertical).");

			PrintFormat("[CGS PORTAL PIP] LIVE test feed bound: color=%1 camera=%2 partner=%3 fps=%4 scale=%5", m_pPortal.GetPortalID(), m_iCameraIndex, m_pPartner.GetPortalID(), fps, scale);
			Print("[CGS PORTAL PIP MATERIAL] If view remains blank, check *_ViewPlane material resolves to CGS_Portal_PIP_Screen_TEST.emat (BCRMap $rendertarget); reimport frame .xob.");
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	// This delay also follows the old CCTV monitor's behavior: wait until
	// the player's world renderer and workspace are fully initialized.
	protected void StartPreviewDelayed()
	{
		m_bStartScheduled = false;
		if (m_bPreviewEnabled && m_pPartner && !m_bInitialized)
			SetupPreview();
	}

	//------------------------------------------------------------------------------------------------
	protected void DestroyPreview()
	{
		// Unbind first. Removing a render target after its material or
		// owner has gone away can crash the engine's render thread.
		if (m_wRTTexture && GetOwner() && !GetOwner().IsDeleted())
			m_wRTTexture.RemoveRenderTarget(GetOwner());

		if (m_wView)
			m_wView.SetWorld(null, m_iCameraIndex);

		if (m_wDebugHUD)
			m_wDebugHUD.RemoveFromHierarchy();

		m_wDebugHUD = null;
		m_wDebugHUDImage = null;

		if (m_wUIRoot)
			m_wUIRoot.RemoveFromHierarchy();

		m_wUIRoot = null;
		m_wRTTexture = null;
		m_wView = null;

		if (m_pFeedCamera)
		{
			delete m_pFeedCamera;
			m_pFeedCamera = null;
		}

		m_bInitialized = false;
		m_bPrintedCameraReadback = false;
		m_fDiagnosticClock = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdatePartnerCamera()
	{
		if (!m_pPartner || !m_pPartner.GetOwner())
			return;

		vector right;
		vector up;
		vector normal;
		m_pPartner.GetPortalBasis(right, up, normal);

		// The prefab's unrotated +Y face points straight UP. A live
		// remote image of empty sky can be mistaken for a black/blank
		// render target. For the FIRST test, look horizontally toward
		// scenery if the portal normal is nearly vertical. This does not
		// change the actual frame or teleport directions.
		if (m_bUseHorizontalTestView && Math.AbsFloat(normal[1]) > 0.85)
		{
			vector partnerTransform[4];
			m_pPartner.GetOwner().GetWorldTransform(partnerTransform);
			vector forwardHorizontal = partnerTransform[2];
			forwardHorizontal[1] = 0;
			float horizontalLength = forwardHorizontal.Length();
			if (horizontalLength > 0.1)
			{
				normal = forwardHorizontal / horizontalLength;
				up = "0 1 0";
				right = Vector(normal[2], 0, -normal[0]);
			}
		}

		// Face OUT of the destination portal, looking at the scene
		// that a traveller would see after exiting there.
		vector cam[4];
		cam[0] = right;
		cam[1] = up;
		cam[2] = normal;

		float outset = m_fCameraOutset;
		if (outset < 0.15)
			outset = 0.15;
		if (outset > 1)
			outset = 1;
		cam[3] = m_pPartner.GetOwner().GetOrigin() + normal * outset;

		if (m_pFeedCamera)
		{
			m_pFeedCamera.SetTransform(cam);
			m_pFeedCamera.FreeFly = false;
		}
		GetOwner().GetWorld().SetCameraEx(m_iCameraIndex, cam);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (!m_pPortal || !m_bPreviewEnabled || !HasLocalRenderer())
			return;

		CGS_PortalComponent opposite = FindOppositeTest();
		if (!opposite)
		{
			if (!m_bLoggedNoPartner && m_bDebug)
			{
				Print("[CGS PORTAL PIP] Waiting for other colored *_PIP_Test portal.");
				m_bLoggedNoPartner = true;
			}

			// If a partner is temporarily missing (GM edit/streaming),
			// tear down only once instead of rebuilding every frame.
			if (m_bInitialized)
				DestroyPreview();

			m_pPartner = null;
			return;
		}

		m_bLoggedNoPartner = false;
		m_pPartner = opposite;

		// Follow the partner transform even if the remote portal moves.
		// This also keeps ScriptCamera.FreeFly disabled every frame.
		if (m_bInitialized)
		{
			UpdatePartnerCamera();

			// One-time readback: the old "feed bound" message only
			// established the widget binding, NOT the camera view.
			m_fDiagnosticClock += timeSlice;
			if (!m_bPrintedCameraReadback && m_fDiagnosticClock >= 1.0)
			{
				m_bPrintedCameraReadback = true;
				if (m_bDebug)
				{
					vector actual[4];
					owner.GetWorld().GetCamera(m_iCameraIndex, actual);
					PrintFormat("[CGS PORTAL PIP CAMERA] World camera READBACK slot=%1 origin=%2 forward=%3 partnerPosition=%4", m_iCameraIndex, actual[3], actual[2], m_pPartner.GetOwner().GetOrigin());
					float outerW;
					float outerH;
					float innerW;
					float innerH;
					m_wRTTexture.GetScreenSize(outerW, outerH);
					m_wView.GetScreenSize(innerW, innerH);
					PrintFormat("[CGS PORTAL PIP LAYOUT] RTTexture0=%1x%2 pixels RenderTarget0=%3x%4 pixels (target >0 in both dimensions)", outerW, outerH, innerW, innerH);
				}
			}
			return;
		}

		// Avoid setting up the second rendering pipeline on the very
		// first game frame. The supplied CCTV mod delays its render
		// target creation for the same reason (renderer readiness).
		if (!m_bStartScheduled)
		{
			m_bStartScheduled = true;
			GetGame().GetCallqueue().CallLater(StartPreviewDelayed, 2200, false);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(StartPreviewDelayed);

		DestroyPreview();
		s_aTests.RemoveItem(this);
		super.OnDelete(owner);
	}
}
