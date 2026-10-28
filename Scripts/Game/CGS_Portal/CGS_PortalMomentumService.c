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
	int m_iArmedAtMs;
	int m_iExpireMs;
	float m_fGravity;
	float m_fMaxStep;
	bool m_bDebug;
	bool m_bTrackOnly;
	int m_iTicks;

	// Offline floor-to-floor prototype. It never uses the legacy
	// all-axis correction or banks momentum from another transfer.
	bool m_bSafeFloorLoop;
	vector m_vLoopStart;

	// Player teleports through SCR_Global can complete AFTER the
	// server TryTransfer returns. Do not start movement at the OLD
	// portal and mistake the engine teleport for player escape.
	bool m_bFloorLoopPendingArrival;
	vector m_vExpectedFloorExit;
	int m_iFloorArrivalTimeoutMs;
	int m_iFloorArrivalSamples;
	int m_iFloorLiftDurationMs;
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

	// Retain the server-side exit velocity from the last portal jump.
	// This is needed because a dedicated server can briefly observe a
	// smaller, interpolated speed than the owning client's launched speed.
	// Only fresh entries qualify; no indefinite accumulation while idle.
	static bool TryGetStackedVelocity(IEntity character, float windowSeconds, out vector stackedVelocity)
	{
		stackedVelocity = vector.Zero;
		if (!Replication.IsServer() || !GetGame() || !GetGame().GetWorld() || !character)
			return false;

		if (windowSeconds <= 0)
			return false;
		if (windowSeconds > 15)
			windowSeconds = 15;

		int now = GetGame().GetWorld().GetWorldTime();
		int windowMs = Math.Round(windowSeconds * 1000);
		foreach (CGS_PortalMomentumState state : s_aActive)
		{
			if (!state || state.m_Character != character)
				continue;
			if (now >= state.m_iExpireMs || now - state.m_iArmedAtMs > windowMs)
				return false;

			stackedVelocity = state.m_vVirtualVelocity;
			return stackedVelocity.LengthSq() > 0.01;
		}

		return false;
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

	// OFFLINE ONLY. Unlike the old high-speed all-axis virtual momentum,
	// this is a small, time-bounded VERTICAL lift after a safe teleport.
	// It stops at the apex and hands falling/walking back to Reforger.
	static void StartFloorLoop(IEntity character, vector expectedExit, float upwardSpeed, float timeoutSeconds, bool debugLog)
	{
		if (!Replication.IsServer() || Replication.IsRunning() || !character)
			return;

		float safeSpeed = Math.Clamp(upwardSpeed, 2.0, 9.0);
		float safeTime = Math.Clamp(timeoutSeconds, 0.4, 2.0);
		StartInternal(character, Vector(0, safeSpeed, 0), safeTime, 9.81, 0.16, false, false);

		foreach (CGS_PortalMomentumState state : s_aActive)
		{
			if (!state || state.m_Character != character)
				continue;

			state.m_bSafeFloorLoop = true;
			state.m_bFloorLoopPendingArrival = true;
			state.m_vExpectedFloorExit = expectedExit;
			state.m_vLoopStart = expectedExit;
			state.m_iFloorArrivalSamples = 0;
			state.m_iFloorLiftDurationMs = Math.Round(safeTime * 1000);
			state.m_iFloorArrivalTimeoutMs = state.m_iArmedAtMs + 650;
			// The flight timeout starts AFTER the character actually
			// arrives. Until then only wait for the safe teleport to
			// resolve, without forcing movement at the old location.
			state.m_iExpireMs = state.m_iFloorArrivalTimeoutMs + 1;
			state.m_bDebug = debugLog;
			if (debugLog)
				PrintFormat("[CGS FLOOR LOOP] Pending arrival character=%1 old=%2 expectedExit=%3 upward=%4",
					character, character.GetOrigin(), expectedExit, safeSpeed);
			return;
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
		state.m_iArmedAtMs = now;
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

			if (state.m_bSafeFloorLoop && state.m_bFloorLoopPendingArrival)
			{
				// This is the SAME asynchronous teleport observed in the
				// user's offline Workbench log: old location immediately,
				// correct destination around the following frame(s).
				if (now >= state.m_iFloorArrivalTimeoutMs)
				{
					if (state.m_bDebug)
						PrintFormat("[CGS FLOOR LOOP] Cancel: teleport arrival timeout. expected=%1 actual=%2",
							state.m_vExpectedFloorExit, character.GetOrigin());
					s_aActive.RemoveOrdered(i);
					continue;
				}

				vector arrivalDelta = character.GetOrigin() - state.m_vExpectedFloorExit;
				// The native character controller may settle 0.6-0.8m
				// lower than the requested teleport origin. Check X/Z
				// more strictly, and allow a short vertical settle.
				if (Math.AbsFloat(arrivalDelta[0]) > 0.85 ||
					Math.AbsFloat(arrivalDelta[2]) > 0.85 ||
					Math.AbsFloat(arrivalDelta[1]) > 1.65)
				{
					state.m_iFloorArrivalSamples = 0;
					continue;
				}

				// Require two near-exit samples, so a single transient
				// transform does not become the flight anchor.
				state.m_iFloorArrivalSamples++;
				if (state.m_iFloorArrivalSamples < 2)
					continue;

				// IMPORTANT: record the position AFTER teleport. The
				// old version used the SOURCE location and immediately
				// canceled when the player moved >3m toward the exit.
				state.m_bFloorLoopPendingArrival = false;
				state.m_vLoopStart = character.GetOrigin();
				state.m_iLastTickMs = now;
				state.m_iArmedAtMs = now;
				state.m_iExpireMs = now + state.m_iFloorLiftDurationMs;
				if (state.m_bDebug)
					PrintFormat("[CGS FLOOR LOOP] Start AFTER ARRIVAL character=%1 origin=%2 expected=%3 upward=%4",
						character, state.m_vLoopStart, state.m_vExpectedFloorExit, state.m_vVirtualVelocity[1]);
				continue;
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

			if (state.m_bSafeFloorLoop)
			{
				// Release control once upward energy is exhausted.
				// Native character gravity then handles the descent.
				if (desired[1] <= 0)
				{
					if (state.m_bDebug)
						PrintFormat("[CGS FLOOR LOOP] Apex: stop lift and return to native gravity. ticks=%1", state.m_iTicks);
					s_aActive.RemoveOrdered(i);
					continue;
				}

				vector floorOrigin = character.GetOrigin();
				vector fromStart = floorOrigin - state.m_vLoopStart;
				fromStart[1] = 0;

				// Walking sideways is a deliberate way to EXIT the loop.
				if (fromStart.LengthSq() > 9.0)
				{
					if (state.m_bDebug)
						Print("[CGS FLOOR LOOP] Cancel: player moved away from exit.");
					s_aActive.RemoveOrdered(i);
					continue;
				}

				float nativeUp = controller.GetVelocity()[1];
				float liftStep = (desired[1] - nativeUp) * dt;
				if (liftStep <= 0.003)
					continue;

				// Hard limit: never move the player sideways or force
				// high-speed multi-meter jumps as old momentum did.
				liftStep = Math.Min(liftStep, 0.16);
				vector nextOrigin = floorOrigin + Vector(0, liftStep, 0);

				TraceSphere safeSweep = new TraceSphere();
				safeSweep.Start = floorOrigin + Vector(0, 0.95, 0);
				safeSweep.End = nextOrigin + Vector(0, 0.95, 0);
				safeSweep.Radius = 0.30;
				safeSweep.Exclude = character;
				safeSweep.Flags = TraceFlags.WORLD | TraceFlags.ENTS;

				if (world.TraceMove(safeSweep, null) < 0.999)
				{
					if (state.m_bDebug)
						Print("[CGS FLOOR LOOP] Cancel: upward path blocked.");
					s_aActive.RemoveOrdered(i);
					continue;
				}

				BaseGameEntity playerEntity = BaseGameEntity.Cast(character);
				if (!playerEntity)
				{
					if (state.m_bDebug)
						Print("[CGS FLOOR LOOP] Cancel: player not a BaseGameEntity.");
					s_aActive.RemoveOrdered(i);
					continue;
				}

				vector floorMatrix[4];
				character.GetWorldTransform(floorMatrix);
				floorMatrix[3] = nextOrigin;
				playerEntity.Teleport(floorMatrix);
				state.m_iTicks++;
				continue;
			}

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
