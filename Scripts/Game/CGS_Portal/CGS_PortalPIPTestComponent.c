//------------------------------------------------------------------------------------------------
// CGS Portal — LOCAL preview-only picture-in-picture proof of concept.
//
// Uses the SAME engine pipeline as Reforger's own PIP weapon optics:
//    RenderTargetWidget.SetWorld(world, cameraIndex)
//    RTTextureWidget.SetRenderTarget(portalEntity)
//    BCRMap + EmissiveMap "$rendertarget" on the ViewPlane material.\n//    Test-only SetObject("$remap...") forces the material on the actual XOB.
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

	[Attribute("1", UIWidgets.CheckBox, "EXCLUSIVE diagnostic: show DIRECT camera on HUD instead of binding portal mesh. Disable this after HUD works to test the portal material.")]
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
	protected RenderTargetWidget m_wDebugDirectCamera;
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
		{
			vector entityWorld[4];
			owner.GetWorldTransform(entityWorld);
			PrintFormat("[CGS PORTAL PIP] Registered test portal color=%1 enable=%2 camera=%3 world=%4 origin=%5", m_pPortal.GetPortalID(), m_bPreviewEnabled, m_iCameraIndex, entityWorld[3], owner.GetOrigin());
		}
	}

	//------------------------------------------------------------------------------------------------
	// For one world-placed prototype pair, choose a LIVE opposite-color
	// portal from this SAME world. Workbench reloads can retain static
	// component references from previous preview / game sessions. Never
	// accept another world or a deleted owner as a partner.
	protected CGS_PortalComponent FindOppositeTest()
	{
		if (!m_pPortal || !GetOwner())
			return null;

		BaseWorld currentWorld = GetOwner().GetWorld();
		vector ourWorld[4];
		GetOwner().GetWorldTransform(ourWorld);

		CGS_PortalComponent closest;
		float closestDistance = 1000000000.0;

		foreach (CGS_PortalPIPTestComponent candidate : s_aTests)
		{
			if (!candidate || candidate == this)
				continue;

			IEntity otherEntity = candidate.GetOwner();
			if (!otherEntity || otherEntity.IsDeleted())
				continue;

			if (otherEntity.GetWorld() != currentWorld)
				continue;

			CGS_PortalComponent other = candidate.m_pPortal;
			if (!other || other.GetPortalID() == m_pPortal.GetPortalID())
				continue;

			if (other.GetPortalOwnerID() != m_pPortal.GetPortalOwnerID())
				continue;

			vector otherWorld[4];
			otherEntity.GetWorldTransform(otherWorld);
			float distance = vector.Distance(ourWorld[3], otherWorld[3]);
			if (!closest || distance < closestDistance)
			{
				closest = other;
				closestDistance = distance;
			}
		}

		return closest;
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasLocalRenderer()
	{
		// Dedicated servers must not create UI widgets or view cameras.
		return GetGame() && GetGame().GetPlayerController() && GetGame().GetWorkspace();
	}

	//------------------------------------------------------------------------------------------------
	// The source material stored in the XOB is not necessarily the
	// final .emat visible at runtime. Workbench can retain a cached
	// imported white placeholder even when XOB.meta contains the correct
	// MaterialAssigns. The official radar demo fixes this with SetObject
	// $remap on the mesh entity. We do the same ONLY for the test ViewPlane.
	//
	// NOTE: Do not call this for the working player portal prefab.
	protected int ForceScreenRenderMaterial()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return 0;

		VObject mesh = owner.GetVObject();
		if (!mesh)
		{
			Print("[CGS PORTAL PIP MATERIAL] ERROR: Test frame entity has no VObject; cannot bind plane material.", LogLevel.ERROR);
			return 0;
		}

		const string targetMaterial = "{6AB25DDA4F021001}Common/Models/Data/CGS_Portal_PIP_Screen_TEST.emat";
		string expectedSource = "CGS_Portal_BLUE_Screen_PLACEHOLDER";
		if (m_pPortal && m_pPortal.GetPortalID() == 1)
			expectedSource = "CGS_Portal_ORANGE_Screen_PLACEHOLDER";

		// Verify the asset is loadable before remapping any slot.
		Resource rtMaterial = Resource.Load(targetMaterial);
		if (!rtMaterial || !rtMaterial.IsValid())
		{
			PrintFormat("[CGS PORTAL PIP MATERIAL] ERROR: Material cannot load: %1", targetMaterial, LogLevel.ERROR);
			return 0;
		}

		string materials[256];
		int count = mesh.GetMaterials(materials);
		int scanCount = count;
		if (scanCount > 256)
			scanCount = 256;

		PrintFormat("[CGS PORTAL PIP MATERIAL] Frame VObject reports %1 material slot(s), expected ViewPlane source='%2'", count, expectedSource);
		string remap;
		bool foundExpected;
		int mapped = 0;

		for (int i = 0; i < scanCount; i++)
		{
			string materialName = materials[i];
			if (materialName.IsEmpty())
				continue;

			PrintFormat("[CGS PORTAL PIP MATERIAL] Source slot[%1]='%2'", i, materialName);

			bool isScreen = materialName == expectedSource;
			if (!isScreen && materialName.Contains("CGS_Portal"))
			{
				isScreen = materialName.Contains("Screen") || materialName.Contains("ViewPlane");
			}

			// Strictly avoid remapping Frame/Metal/Glow slots.
			if (!isScreen)
				continue;

			remap += string.Format("$remap '%1' '%2';", materialName, targetMaterial);
			mapped++;

			if (materialName == expectedSource)
				foundExpected = true;
		}

		// If GetMaterials only exposes the resolved .emat rather than the
		// original FBX material slot, try the known source name as well.
		if (!foundExpected)
		{
			remap += string.Format("$remap '%1' '%2';", expectedSource, targetMaterial);
			PrintFormat("[CGS PORTAL PIP MATERIAL] Expected XOB source not in GetMaterials; adding explicit fallback: %1", expectedSource);
			mapped++;
		}

		owner.SetObject(mesh, remap);
		PrintFormat("[CGS PORTAL PIP MATERIAL] Runtime XOB screen remap issued: count=%1 target=%2", mapped, targetMaterial);
		return mapped;
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

		// Step A: direct-HUD diagnostic. Render the camera on ONLY ONE
		// RenderTargetWidget, not both the HUD and mesh at once.
		// The old CCTV author warned about native rendering crashes
		// when too many targets were active simultaneously.
		// Step B: disable m_bDebugHUD to bind the world view to the
		// portal mesh material once the direct camera is proven.
		float scale = Math.Clamp(m_fResolutionScale, 0.2, 1.0);
		m_wView.SetResolutionScale(scale, scale);
		if (m_bDebugHUD)
			m_wView.SetVisible(false);
		else
		{
			m_wView.SetWorld(world, m_iCameraIndex);
			m_wView.SetVisible(true);
		}

		int fps = m_iMaxFPS;
		if (fps < 1)
			fps = 1;
		if (fps > 30)
			fps = 30;
		m_wView.SetMaxFPS(fps);
		m_wView.SetRefresh(1, 0);

		// Mesh pipeline and diagnostic HUD are intentionally mutually
		// exclusive until one has proven live world pixels.
		// To test the plane material, disable m_bDebugHUD in the
		// Blue_Portal_PIP_Test.et component in Workbench.
		if (!m_bDebugHUD)
		{
			// Force the imported XOB's screen material slot to the actual
			// runtime render-target material BEFORE binding the entity.
			// Old camera/UI reports alone cannot verify this assignment.
			int remapped = ForceScreenRenderMaterial();
			if (remapped < 1)
				Print("[CGS PORTAL PIP MESH] WARNING: No screen material remap was applied. Check imported ViewPlane material.", LogLevel.WARNING);

			m_wRTTexture.SetRenderTarget(GetOwner());
			PrintFormat("[CGS PORTAL PIP MESH] Render target bound AFTER XOB screen material remap (%1 slot attempts); camera diagnostic HUD disabled.", remapped);
		}
		else
		{
			Print("[CGS PORTAL PIP HUD] EXCLUSIVE diagnostic mode. In-world portal plane is intentionally NOT receiving a camera feed in this test.");
		}

		// Previous HUD used ImageWidget.SetImageTexture() and only drew
		// a white rectangle. That is ambiguous: the image may be invalid
		// despite the world camera rendering. This diagnostic uses a
		// separate RenderTargetWidget which draws the world camera DIRECTLY.
		// It bypasses RTTexture0 and the mesh material entirely.
		if (m_bDebugHUD)
		{
			m_wDebugHUD = GetGame().GetWorkspace().CreateWidgets(m_sDebugHUDLayout);
			if (m_wDebugHUD)
			{
				m_wDebugDirectCamera = RenderTargetWidget.Cast(m_wDebugHUD.FindAnyWidget("PIPDebugCamera"));
				if (m_wDebugDirectCamera)
				{
					m_wDebugDirectCamera.SetWorld(world, m_iCameraIndex);
					m_wDebugDirectCamera.SetResolutionScale(scale, scale);
					m_wDebugDirectCamera.SetMaxFPS(fps);
					m_wDebugDirectCamera.SetRefresh(1, 0);
					m_wDebugDirectCamera.SetVisible(true);
					Print("[CGS PORTAL PIP HUD] DIRECT world-camera widget created; top-left preview now bypasses RTTexture0 and the portal material.");
				}
				else
				{
					Print("[CGS PORTAL PIP HUD] WARNING: PIPDebugCamera not found in diagnostic HUD layout.");
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
			vector blueWorld[4];
			vector orangeWorld[4];
			GetOwner().GetWorldTransform(blueWorld);
			m_pPartner.GetOwner().GetWorldTransform(orangeWorld);
			PrintFormat("[CGS PORTAL PIP PAIR] blueWorld=%1 blueGetOrigin=%2 orangeWorld=%3 orangeGetOrigin=%4", blueWorld[3], GetOwner().GetOrigin(), orangeWorld[3], m_pPartner.GetOwner().GetOrigin());
			PrintFormat("[CGS PORTAL PIP CAMERA] ScriptCamera CREATED slot=%1 worldOrigin=%2 forward=%3 partnerWorld=%4", m_iCameraIndex, lens[3], lens[2], orangeWorld[3]);

			vector remoteForward = lens[2];
			if (Math.AbsFloat(remoteForward[1]) >= 0.85)
				Print("[CGS PORTAL PIP CAMERA] NOTE: Camera is vertical; rotate remote test frame or enable horizontal preview.");
			else
				Print("[CGS PORTAL PIP CAMERA] Camera aimed toward scenery (not vertical).");

			if (m_bDebugHUD)
				PrintFormat("[CGS PORTAL PIP] DIRECT HUD camera active: color=%1 camera=%2 partner=%3 fps=%4 scale=%5", m_pPortal.GetPortalID(), m_iCameraIndex, m_pPartner.GetPortalID(), fps, scale);
			else
			{
				PrintFormat("[CGS PORTAL PIP] LIVE mesh feed bound: color=%1 camera=%2 partner=%3 fps=%4 scale=%5", m_pPortal.GetPortalID(), m_iCameraIndex, m_pPartner.GetPortalID(), fps, scale);
				Print("[CGS PORTAL PIP MATERIAL] If still blank, inspect source slot list above: the imported mesh may have a separate ViewPlane entity, stale material cache, or reversed normals.");
			}
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
		// Release the independent direct world-camera view first.
		if (m_wDebugDirectCamera)
			m_wDebugDirectCamera.SetWorld(null, m_iCameraIndex);

		if (m_wDebugHUD)
			m_wDebugHUD.RemoveFromHierarchy();

		m_wDebugHUD = null;
		m_wDebugDirectCamera = null;

		// Then unbind the mesh material before removing the render-target
		// layout and ScriptCamera (native render thread owns these refs).
		if (m_wRTTexture && GetOwner() && !GetOwner().IsDeleted())
			m_wRTTexture.RemoveRenderTarget(GetOwner());

		if (m_wView)
			m_wView.SetWorld(null, m_iCameraIndex);

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
		vector partnerWorld[4];
		m_pPartner.GetOwner().GetWorldTransform(partnerWorld);
		cam[3] = partnerWorld[3] + normal * outset;

		if (m_pFeedCamera)
		{
			m_pFeedCamera.SetWorldTransform(cam);
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
					vector currentPartnerWorld[4];
					m_pPartner.GetOwner().GetWorldTransform(currentPartnerWorld);
					PrintFormat("[CGS PORTAL PIP CAMERA] World camera READBACK slot=%1 origin=%2 forward=%3 partnerWorld=%4", m_iCameraIndex, actual[3], actual[2], currentPartnerWorld[3]);
					float outerW;
					float outerH;
					float innerW;
					float innerH;
					m_wRTTexture.GetScreenSize(outerW, outerH);
					m_wView.GetScreenSize(innerW, innerH);
					PrintFormat("[CGS PORTAL PIP LAYOUT] RTTexture0=%1x%2 pixels RenderTarget0=%3x%4 pixels (target >0 in both dimensions)", outerW, outerH, innerW, innerH);
					if (m_wDebugDirectCamera)
					{
						float hudWidth;
						float hudHeight;
						m_wDebugDirectCamera.GetScreenSize(hudWidth, hudHeight);
						PrintFormat("[CGS PORTAL PIP HUD] DirectCamera=%1x%2 pixels; independently rendering world slot %3", hudWidth, hudHeight, m_iCameraIndex);
					}
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
