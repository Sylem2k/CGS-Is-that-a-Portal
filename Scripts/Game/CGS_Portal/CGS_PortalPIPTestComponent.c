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

	[Attribute("{EF091399D840192D}UI/layouts/Sights/PictureInPictureSightsLayout.layout", UIWidgets.ResourceNamePicker, "Use engine PIP scope layout (contains RTTexture0 and RenderTarget0)", "layout")]
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

	protected static ref array<CGS_PortalPIPTestComponent> s_aTests = {};

	protected CGS_PortalComponent m_pPortal;
	protected CGS_PortalComponent m_pPartner;

	protected ref Widget m_wUIRoot;
	protected RTTextureWidget m_wRTTexture;
	protected RenderTargetWidget m_wView;
	protected bool m_bLoggedNoPartner;
	protected bool m_bInitialized;

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
			PrintFormat("[CGS PORTAL PIP] Invalid camera index %1 (valid 1-31)", m_iCameraIndex);
			return false;
		}

		m_wUIRoot = GetGame().GetWorkspace().CreateWidgets(m_sPIPLayout);
		if (!m_wUIRoot)
		{
			Print("[CGS PORTAL PIP] Failed to create stock PictureInPictureSightsLayout.layout.");
			return false;
		}

		m_wRTTexture = RTTextureWidget.Cast(m_wUIRoot.FindAnyWidget("RTTexture0"));
		m_wView = RenderTargetWidget.Cast(m_wUIRoot.FindAnyWidget("RenderTarget0"));
		if (!m_wRTTexture || !m_wView)
		{
			Print("[CGS PORTAL PIP] Stock PIP layout missing RTTexture0 or RenderTarget0.");
			DestroyPreview();
			return false;
		}

		BaseWorld world = GetOwner().GetWorld();
		float fov = m_fCameraFOV;
		if (fov < 25)
			fov = 25;
		if (fov > 110)
			fov = 110;

		world.SetCameraType(m_iCameraIndex, CameraType.PERSPECTIVE);
		world.SetCameraVerticalFOV(m_iCameraIndex, fov);
		world.SetCameraNearPlane(m_iCameraIndex, 0.10);
		world.SetCameraFarPlane(m_iCameraIndex, 600);

		// The PIP scope uses precisely this two-widget render pipeline.
		m_wView.SetWorld(world, m_iCameraIndex);
		float scale = Math.Clamp(m_fResolutionScale, 0.2, 1.0);
		m_wView.SetResolutionScale(scale, scale);

		int fps = m_iMaxFPS;
		if (fps < 1)
			fps = 1;
		if (fps > 30)
			fps = 30;
		m_wView.SetMaxFPS(fps);
		m_wView.SetRefresh(1, 0);

		// This affects ONLY the mesh sections using BCRMap "$rendertarget".
		// Our test prefab points its screen material at that shader, while
		// the metal frame and glow keep their existing materials.
		m_wRTTexture.SetRenderTarget(GetOwner());

		m_bInitialized = true;
		if (m_bDebug)
			PrintFormat("[CGS PORTAL PIP] LIVE test feed bound: color=%1 camera=%2 partner=%3 fps=%4 scale=%5", m_pPortal.GetPortalID(), m_iCameraIndex, m_pPartner.GetPortalID(), fps, scale);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void DestroyPreview()
	{
		if (m_wRTTexture && GetOwner() && !GetOwner().IsDeleted())
			m_wRTTexture.RemoveRenderTarget(GetOwner());

		if (m_wView)
			m_wView.SetWorld(null, m_iCameraIndex);

		if (m_wUIRoot)
			m_wUIRoot.RemoveFromHierarchy();

		m_wUIRoot = null;
		m_wRTTexture = null;
		m_wView = null;
		m_bInitialized = false;
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

			if (m_bInitialized)
				DestroyPreview();

			m_pPartner = null;
			return;
		}

		m_bLoggedNoPartner = false;
		m_pPartner = opposite;

		// Set the camera position BEFORE target setup so the first
		// allocated render isn't from world origin.
		UpdatePartnerCamera();

		if (!m_bInitialized)
			SetupPreview();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		DestroyPreview();
		s_aTests.RemoveItem(this);
		super.OnDelete(owner);
	}
}
