//------------------------------------------------------------------------------------------------
// CGS Pinkifier - SAFE character clothing color helper.
//
// DO NOT use SCR_Global.SetMaterial(character, material, true) here.
// That helper replaces VObjects by calling SetObject() recursively
// across skinned characters and attached inventory/equipment.
// In the first Pinkifier test, the game crashed when the broadcast
// handler attempted to remap a target's complete character hierarchy.
//
// Instead, use the engine's existing ParametricMaterialInstanceComponent,
// which clothing itself uses for user-controlled material parameters
// such as blood. SetColor() changes the ALBEDO INSTANCE COLOR in place.
// It does not replace meshes, material resources, animation rigs,
// inventories, physics objects or network entity identity.
//
// Only touch EQUIPPED clothing slots returned by
// EquipedLoadoutStorageComponent.GetClothFromArea(), not all children
// (a character can have hundreds of inventory child entities).
//
// This is a deliberately conservative Phase 2 fallback. Some custom
// clothes may lack the component or material support for color tint;
// they are skipped instead of trying an unsafe $remap.
//------------------------------------------------------------------------------------------------
class CGS_PinkVisuals
{
	// Returns 1 if this entity has an eligible color-instance component.
	// NO VObject.SetObject; no recursively changing entities.
	protected static int TintOneEntity(IEntity entity)
	{
		if (!entity || entity.IsDeleted())
			return 0;

		ParametricMaterialInstanceComponent instance = ParametricMaterialInstanceComponent.Cast(entity.FindComponent(ParametricMaterialInstanceComponent));
		if (!instance)
			return 0;

		// API: ParametricMaterialInstanceComponent.SetColor(int argbColor).
		// Color.PINK is a stock Reforger ARGB color constant.
		instance.SetColor(Color.PINK);
		return 1;
	}

	// The character and its equipped clothes are already present
	// on each client when the target's replicated entity resolves.
	// No native material resource replacements are performed.
	static int TintCharacter(ChimeraCharacter character)
	{
		if (!character || character.IsDeleted())
			return 0;

		int painted = 0;
		int slotsFound = 0;

		// Some character prefabs support a material instance directly.
		// If absent, do not attempt a mesh/material replacement.
		painted += TintOneEntity(character);

		EquipedLoadoutStorageComponent equipped = EquipedLoadoutStorageComponent.Cast(character.FindComponent(EquipedLoadoutStorageComponent));
		if (!equipped)
		{
			Print("[CGS PINKIFIER SAFE] Character has no equipped-loadout component; skipped clothing tint.", LogLevel.WARNING);
			return painted;
		}

		// Use the same clothing entity retrieval used by vanilla
		// Reforger SCR_CharacterDamageManagerComponent.AddBloodToClothes.
		// Only inspect worn gear, never the entire item inventory.
		IEntity jacket = equipped.GetClothFromArea(LoadoutJacketArea);
		IEntity pants = equipped.GetClothFromArea(LoadoutPantsArea);
		IEntity vest = equipped.GetClothFromArea(LoadoutVestArea);
		IEntity headwear = equipped.GetClothFromArea(LoadoutHeadCoverArea);
		IEntity boots = equipped.GetClothFromArea(LoadoutBootsArea);
		IEntity gloves = equipped.GetClothFromArea(LoadoutHandwearSlotArea);
		IEntity backpack = equipped.GetClothFromArea(LoadoutBackpackArea);

		// The items may be absent, which is normal. A missing slot
		// doesn't mean an invalid character or material failure.
		if (jacket)
		{
			slotsFound++;
			painted += TintOneEntity(jacket);
		}
		if (pants)
		{
			slotsFound++;
			painted += TintOneEntity(pants);
		}
		if (vest)
		{
			slotsFound++;
			painted += TintOneEntity(vest);
		}
		if (headwear)
		{
			slotsFound++;
			painted += TintOneEntity(headwear);
		}
		if (boots)
		{
			slotsFound++;
			painted += TintOneEntity(boots);
		}
		if (gloves)
		{
			slotsFound++;
			painted += TintOneEntity(gloves);
		}
		if (backpack)
		{
			slotsFound++;
			painted += TintOneEntity(backpack);
		}

		PrintFormat("[CGS PINKIFIER SAFE] Color applied to %1 material-instance component(s), %2 equipped clothing slot(s) found. No VObject remaps.", painted, slotsFound);

		if (painted == 0)
			Print("[CGS PINKIFIER SAFE] No supported material-instance components on this outfit. Target left unchanged to avoid another native crash.", LogLevel.WARNING);

		return painted;
	}
}
