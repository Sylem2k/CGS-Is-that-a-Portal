//------------------------------------------------------------------------------------------------
// CGS Portal - experimental momentum carry for normal player characters.
//
// The Reforger character controller exposes GetVelocity(), but not a matching
// SetVelocity(). The player's Physics body is non-dynamic, so SetVelocity()
// does not launch that character.
//
// Keep a virtual desired velocity and compensate for the difference from native
// character-controller movement. In multiplayer the OWNER applies the correction
// to its own predicted character; the server keeps velocity for portal chaining.
// A local/listen host still uses the server movement path. This is a swept
// position assist, NOT true physics velocity or collision immunity.
//------------------------------------------------------------------------------------------------
class CGS_PortalMomentumState : Managed
{
	IEntity m_Character;
	vector m_vVirtualVelocity;
	int m_iLastTickMs;
	int m_iExpireMs;
	float m_fGravity;
	float m_fMaxStep;
	bool m_bDebug;
	bool m_bTrackOnly;
	int m_iTicks;
}

//------------------------------------------------------------------------------------------------
class CGS_PortalMomentumService
{
	protected static ref array<ref CGS_PortalMomentumState> s_aActive = {};
	protected static int s_iLastServiceTick = -1;
	protected static int s_iMomentumArms;

	// Read our carried momentum when chaining portals; controller velocity may
	// have been reset by SCR_Global.TeleportPlayer between the portals.
	static vector GetEffectiveVelocity(IEntity character, vector fallback)
	{
		if (!character)
			return fallback;

		foreach (CGS_PortalMomentumState state : s_aActive)
		{
			if (state && state.m_Character == character)
				return state.m_vVirtualVelocity;
		}

		return fallback;
	}

	static void Stop(IEntity character)
	{
		for (int i = s_aActive.Count() - 1; i >= 0; i--)
		{
			CGS_PortalMomentumState state = s_aActive[i];
			if (state && state.m_Character == character)
				s_aActive.RemoveOrdered(i);
		}
	}

	// Authoritative server remembers the virtual velocity for chaining.
	// For remote MP players it does not fight the owner's predicted movement.
	static void Start(IEntity character, vector wantedVelocity, float durationSeconds, float gravity, float maxStep, bool debugLog, bool trackOnly)
	{
		if (!Replication.IsServer() || !character)
			return;

		StartInternal(character, wantedVelocity, durationSeconds, gravity, maxStep, debugLog, trackOnly);
	}

	// Called only by the reliable Owner RPC after the client's actual
	// teleport. The owner runs the same swept momentum steps locally.
	static void StartOwner(IEntity character, vector wantedVelocity, float durationSeconds, float gravity, float maxStep, bool debugLog)
	{
		if (Replication.IsServer() || !GetGame() || !character)
			return;

		PlayerController localController = GetGame().GetPlayerController();
		if (!localController || localController.GetControlledEntity() != character)
			return;

		StartInternal(character, wantedVelocity, durationSeconds, gravity, maxStep, debugLog, false);
	}

	protected static void StartInternal(IEntity character, vector wantedVelocity, float durationSeconds, float gravity, float maxStep, bool debugLog, bool trackOnly)
	{
		Stop(character);
		if (!GetGame() || !GetGame().GetWorld() || durationSeconds <= 0 || wantedVelocity.LengthSq() < 0.01)
			return;

		if (durationSeconds > 15)
			durationSeconds = 15;
		if (gravity < 0)
			gravity = 0;
		if (gravity > 30)
			gravity = 30;
		if (maxStep < 0.05)
			maxStep = 0.05;
		if (maxStep > 2)
			maxStep = 2;

		int now = GetGame().GetWorld().GetWorldTime();
		ref CGS_PortalMomentumState state = new CGS_PortalMomentumState();
		state.m_Character = character;
		state.m_vVirtualVelocity = wantedVelocity;
		state.m_iLastTickMs = now;
		state.m_iExpireMs = now + Math.Round(durationSeconds * 1000);
		state.m_fGravity = gravity;
		state.m_fMaxStep = maxStep;
		state.m_bDebug = debugLog;
		state.m_bTrackOnly = trackOnly;
		s_aActive.Insert(state);

		s_iMomentumArms++;
		if (debugLog)
			PrintFormat("[CGS MOMENTUM MP] Arm #%1 server=%2 trackOnly=%3 velocity=%4 duration=%5s gravity=%6",
				s_iMomentumArms, Replication.IsServer(), trackOnly, wantedVelocity, durationSeconds, gravity);
	}

	// Server: track virtual velocity, move only a locally controlled host.
	// Runs once per server frame, even when both portals call it.
	static void Update()
	{
		if (!Replication.IsServer())
			return;

		UpdateInternal();
	}

	// Owning client: called once per rendered frame by SCR_PlayerController.
	// This mirrors the teleport owner-RPC path already proven in MP.
	static void UpdateOwner()
	{
		if (Replication.IsServer() || !GetGame() || !GetGame().GetPlayerController())
			return;

		UpdateInternal();
	}

	protected static void UpdateInternal()
	{
		if (!GetGame() || !GetGame().GetWorld())
			return;

		int now = GetGame().GetWorld().GetWorldTime();
		if (now == s_iLastServiceTick)
			return;
		s_iLastServiceTick = now;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		for (int i = s_aActive.Count() - 1; i >= 0; i--)
		{
			CGS_PortalMomentumState state = s_aActive[i];
			if (!state || !state.m_Character || now >= state.m_iExpireMs)
			{
				s_aActive.RemoveOrdered(i);
				continue;
			}

			ChimeraCharacter character = ChimeraCharacter.Cast(state.m_Character);
			if (!character)
			{
				s_aActive.RemoveOrdered(i);
				continue;
			}

			CharacterControllerComponent controller = character.GetCharacterController();
			if (!controller || controller.IsDead())
			{
				s_aActive.RemoveOrdered(i);
				continue;
			}

			// Never let another client's static momentum state move this
			// machine's predicted character.
			if (!Replication.IsServer())
			{
				PlayerController localController = GetGame().GetPlayerController();
				if (!localController || localController.GetControlledEntity() != character)
				{
					s_aActive.RemoveOrdered(i);
					continue;
				}
			}

			int elapsedMs = now - state.m_iLastTickMs;
			if (elapsedMs <= 0)
				continue;

			state.m_iLastTickMs = now;

			// Avoid a huge corrective jump after an unexpected server freeze.
			if (elapsedMs > 150)
			{
				if (state.m_bDebug)
					PrintFormat("[CGS MOMENTUM] Large frame gap (%1ms). Ending virtual momentum safely.", elapsedMs);
				s_aActive.RemoveOrdered(i);
				continue;
			}

			float dt = elapsedMs * 0.001;
			vector desired = state.m_vVirtualVelocity;
			desired[1] = desired[1] - state.m_fGravity * dt;
			state.m_vVirtualVelocity = desired;

			// Server keeps an approximate carried velocity for multi-portal
			// chains; the owning client supplies actual movement in MP.
			if (state.m_bTrackOnly)
				continue;

			// Native walking/falling movement is already being simulated.
			// Move only by the DELTA needed to redirect it to portal velocity.
			vector nativeVelocity = controller.GetVelocity();
			vector correction = (desired - nativeVelocity) * dt;
			float distance = correction.Length();
			if (distance < 0.004)
				continue;

			if (distance > state.m_fMaxStep)
				correction = correction * (state.m_fMaxStep / distance);

			vector origin = character.GetOrigin();
			vector target = origin + correction;

			// Do not push underground, but DO allow horizontal launch
			// corrections while feet are already at ground height. The
			// previous implementation cancelled all momentum for a small
			// downward component, even with a clear horizontal path.
			float groundY = world.GetSurfaceY(target[0], target[2]);
			if (target[1] < groundY + 0.15)
			{
				if (origin[1] >= groundY + 0.15)
					target[1] = groundY + 0.15;
				else
					target[1] = origin[1];

				correction = target - origin;
				if (correction.LengthSq() < 0.000016)
					continue;
			}

			// Conservative spherical sweep around the character torso.
			// If any obstacle lies on the correction path, do not force
			// a teleport through it. Still no global collision immunity.
			TraceSphere sweep = new TraceSphere();
			sweep.Start = origin + Vector(0, 0.95, 0);
			sweep.End = target + Vector(0, 0.95, 0);
			sweep.Radius = 0.32;
			sweep.Exclude = character;
			sweep.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
			if (world.TraceMove(sweep, null) < 0.999)
			{
				if (state.m_bDebug)
					Print("[CGS MOMENTUM] Stopped: correction path is blocked.");
				s_aActive.RemoveOrdered(i);
				continue;
			}

			// BaseGameEntity.Teleport is what Reforger's own teleport
			// implementation uses for entity movement. Only apply small
			// per-frame offsets, not high-speed SetOrigin jumps.
			vector matrix[4];
			character.GetWorldTransform(matrix);
			matrix[3] = target;
			BaseGameEntity baseEntity = BaseGameEntity.Cast(character);
			if (!baseEntity)
			{
				s_aActive.RemoveOrdered(i);
				continue;
			}

			baseEntity.Teleport(matrix);
			state.m_iTicks++;

			if (state.m_bDebug && (s_iMomentumArms <= 3 || s_iMomentumArms % 10 == 0) && (state.m_iTicks == 1 || state.m_iTicks % 60 == 0))
				PrintFormat("[CGS MOMENTUM MP] tick=%1 server=%2 wanted=%3 controller=%4 correction=%5", state.m_iTicks, Replication.IsServer(), desired, nativeVelocity, correction);
		}
	}
}
