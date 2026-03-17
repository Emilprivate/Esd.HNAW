#include "features/aimbot/weapon_type_names.h"

namespace Aimbot {
    enum class WeaponType : uint16_t {
        Unarmed = 0,
        CannonRamRodRammingObject,
        Sword_1796_Epee,
        Sword_1805_Epee,
        Sword_Sabre_Officier_Infanterie,
        BroadsideCannon = 12,
        CarronadeCannon,
        SwivelCannon,
        CoastalCannon,
        Cannon24Pounder,
        MortarCannon,
        BuckShotHowitzer = 19,
        FieldHowitzer,
        Cannon4Pounder,
        BearingFlag,
        FieldCannon9Pounder,
        Cannon4PounderWheelCarriage,
        Carronade_RotatingCannonCarriage,
        CannonRamRod,
        CannonRoundShot,
        BuckShot,
        RocketArtillery,
        DeconstructingHammer,
        ConstructingHammer,
        DiggingSpade,
        BuckShotSmall,
        BuckShotLarge,
        Depreacted_Musket_SeaServiceBrownBess,
        Deprecated_Musket_CharlevilleMarinePatternANIX,
        Deprecated_Musket_IndiaPatternBrownBess,
        Musket_Charleville_Musket_IXXIII_Standard_Year,
        Musket_Land_Pattern_Musket_Brown_Bess,
        Deprecated_Musket_CharlevilleGuardPattern,
        Deprecated_Musket_NewLandLightInfantryPatternBrownBess,
        Deprecated_Musket_CharlevilleDragoonPattern,
        Carbine_Elliot_Carbine,
        Carbine_Charleville_Cavalry_Carbine_IXXIII,
        Rifle_Baker_Rifle_Pattern_1800 = 46,
        Depreacted_Rifle_CharlevilleDragoonPattern_NoBayonet,
        Deprecated_Pistol_SeaService,
        Pistol_French_Pistol_XIII,
        Pistol_Land_Pattern_Pistol,
        Blunderbuss_French_Blunderbuss = 52,
        Deprecated_Blunderbuss_Variation2,
        Axe2H_Variation1,
        Axe2H_British_2Handed_Axe,
        Axe1H_Carpenter_Adze,
        Axe1H_Carpenter_Axe,
        Deprecated_Sword_Pattern1803FlankOfficer,
        Sword_An_XI_Light_Sabre = 62,
        Sword_1715_Pattern_Sabre,
        Deprecated_Sabre_AnXILight,
        Deprecated_Sabre_1796PatternLight,
        Sword_Baker_Rifle_Bayonet,
        Sword_Sabre_Briquet,
        Sword_British_Hanger,
        Sword_French_Hanger,
        Sword_1804_Pattern_Naval_Cutlass,
        Sword_Year_IX_Boarding_Sabre,
        Knife_British_Dagger,
        Knife_French_Dagger,
        Knife_British_Knife,
        Knife_French_Knife,
        Spontoon_British_Boarding_Pike,
        Spontoon_French_Boarding_Pike,
        ExplosiveBarrel,
        Sword_Pattern_1803_Flank_Officer_Sabre,
        Sword_1796_Pattern_Light_Sabre,
        Sword_Russian_Epee_Sword = 82,
        Deprecated_Sword_Russian_Epee_Officer_Sword,
        Sword_Russian_Hussar_Sword,
        Sword_Russian_Cuirassier_Sword,
        Sword_Russian_Rifle_Sword,
        Pistol_Russian_1809_Pistol,
        Musket_Russian_Musket_Pattern_1808,
        Deprecated_Musket_Russian_1808_Guard,
        Deprecated_Musket_Russian_1808_Light,
        Rifle_Russian_Rifle_Pattern_1806,
        Carbine_Russian_Cuirassier_Carbine,
        Sword_Claymore,
        Frontlines_MachineGun_Allied = 101,
        Frontlines_MachineGun_Central,
        Frontlines_LeeEnfield_MKIII = 100,
        Frontlines_Mauser_Gewehr_98 = 103,
        Frontlines_M1903_Springfield,
        Frontlines_Lebel_Model_1886,
        Frontlines_Webley_Revolver_MKIV,
        Frontlines_Modele_1892_Revolver,
        Frontlines_M1917_Revolver,
        Frontlines_Mauser_C78_Revolver,
        Frontlines_Pattern_1897_Officer_Sword,
        Frontlines_M1882_Sabre_d_officier,
        Frontlines_Model_1902_Army_Officers_Sword,
        Frontlines_M1889_infantrie_saebel,
        Frontlines_British_Trench_Club,
        Frontlines_French_Trench_Club,
        Frontlines_German_Trench_Club,
        Frontlines_P1907_Trench_Knife,
        Frontlines_M1916_Vengeur_Knife,
        Frontlines_1918_Trench_Knife,
        Frontlines_German_Boker_Knife,
        Frontlines_Allied_Shovel,
        Frontlines_CentralPower_Shovel,
        Frontlines_LeeEnfield_MKIII_No_Bayonet = 140,
        Frontlines_Mauser_Gewehr_98_No_Bayonet,
        Frontlines_M1903_Springfield_No_Bayonet,
        Frontlines_Lebel_Model_1886_No_Bayonet,
        Frontlines_MillsGrenade_MKII = 151,
        Frontlines_Stielhandgranate_1916,
        Frontlines_No37_MKI,
        Frontlines_HandNebelBombe,
        Frontlines_Howitzer,
        Frontlines_HeavyMortar,
        Frontlines_TNT,
        Frontlines_HeavyHowitzer,
        Frontlines_ShipTurret,
        Unused_Frontlines_GreaseGun,
        Unused_Frontlines_TrenchGun,
        Frontlines_3InchDeckGun,
        Frontlines_CoastGun,
        Unarmed_Surrender = 200,
        Musket_Shoulder_Arms_Gesture,
        Deprecated_Musket_SeaServiceBrownbess_NoBayonet,
        Deprecated_Musket_LandLightInfantryPatternBrownBess_NoBayonet,
        Deprecated_Musket_1808PatternMusket_NoBayonet,
        Deprecated_Musket_IndiaPatternBrownBess_NoBayonet,
        Deprecated_Musket_CharlevilleMusketIX_XIII_NoBayonet,
        Deprecated_Musket_1808PatternMusket_LightVariant_NoBayonet,
        Deprecated_Musket_CharlevilleMusketMarinePatternAnIX_NoBayonet,
        Deprecated_Musket_CharlevilleMusketDragoonPattern_NoBayonet,
        Spontoon_Russian_Spontoon,
        Spontoon_Prussian_Spontoon,
        Spontoon_French_Spontoon,
        Spontoon_British_Spontoon,
        BroadsideCannon_Small,
        Unarmed_Zombie,
        ChristmasConfettiBuckshot,
        ChristmasConfettiBuckshotSmall,
        Napoleon_Waistcoat,
        Napoleon_BehindBack,
        Napoleon_Charge,
        Musket_Charleville_Musket_Dragoon_Pattern_Rusty,
        Sword_Scimitar,
        Sword_Napoleons_Sword,
        Sword_Yatagan_Sword,
        Sword_Frying_Pan,
        Axe2H_Sapper_2Handed_Axe,
        Spontoon_Halberd,
        Musket_Charleville_Musket_Guard_Pattern_Decorated,
        Musket_Land_Light_Infantry_Pattern_Musket_Brown_Bess_Rusty,
        Musket_Sea_Service_Musket_Brown_Bess_Decorated,
        Musket_Russian_Musket_Pattern_1808_Light_Pattern_Rusty,
        Musket_Russian_Musket_Pattern_1808_Guard_Pattern_Decorated,
        Musket_Spanish_Escopeta_Musket,
        Musket_Spanish_Escopeta_Musket_Decorated,
        Musket_Ottoman_Tufenk_Musket,
        Musket_Ottoman_Tufenk_Musket_Decorated,
        Musket_Boutet_Musket,
        Musket_Boutet_Musket_Decorated,
        Rifle_Baker_Rifle_Pattern_1800_Rusty,
        Rifle_Baker_Rifle_Pattern_1800_Decorated,
        Rifle_Russian_Rifle_Pattern_1806_Rusty,
        Rifle_Russian_Rifle_Pattern_1806_Decorated,
        Rifle_Sporting_Rifle,
        Rifle_Sporting_Rifle_Decorated,
        Carbine_Charleville_Cavalry_Carbine_IXXIII_Rusty,
        Carbine_Charleville_Cavalry_Carbine_IXXIII_Decorated,
        Carbine_Elliot_Carbine_Rusty,
        Carbine_Elliot_Carbine_Decorated,
        Carbine_Russian_Cuirassier_Carbine_Rusty,
        Carbine_Russian_Cuirassier_Carbine_Decorated,
        Carbine_Sporting_Carbine,
        Carbine_Sporting_Carbine_Decorated,
        Pistol_Land_Pattern_Pistol_Rusty,
        Pistol_Sea_Service_Pistol_Decorated,
        Pistol_French_Pistol_XIII_Rusty,
        Pistol_French_Pistol_XIII_Decorated,
        Pistol_Russian_1809_Pistol_Rusty,
        Pistol_Russian_1809_Pistol_Decorated,
        Pistol_Le_Page_Pistol,
        Pistol_Le_Page_Pistol_Decorated,
        Blunderbuss_British_Blunderbuss_Rusty,
        Blunderbuss_French_Blunderbuss_Decorated,
        Blunderbuss_Nock_Gun_Blunderbuss,
        Blunderbuss_Nock_Gun_Blunderbuss_Decorated,
        Musket_Iroquois_Musket,
        Sword_Frying_Pan_Gold,
        Deprecated_Frontlines_Frying_Pan,
        Deprecated_Frontlines_Frying_Pan_Gold,
        Frontlines_Lebel_Model_1886_Decorated,
        Frontlines_Lebel_Model_1886_Rusty,
        Frontlines_LeeEnfield_MKIII_Decorated,
        Frontlines_LeeEnfield_MKIII_Rusty,
        Frontlines_M1903_Springfield_Decorated,
        Frontlines_M1903_Springfield_Rusty,
        Frontlines_Mauser_Gewehr_98_Decorated,
        Frontlines_Mauser_Gewehr_98_Rusty,
        Frontlines_M1897_Trench_Gun,
        Frontlines_M1897_Trench_Gun_Warthorn,
        Frontlines_M1897_Trench_Gun_Decorated,
        Sword_Whiskey_Bottle,
        Sword_Whiskey_Bottle_Broken,
        Sword_Whiskey_Bottle_Decorated,
        Sword_Rum_Bottle,
        Sword_Rum_Bottle_Broken,
        Sword_Rum_Bottle_Decorated,
        Musket_Tartan_Wrapped,
        Sword_Nelson,
        Pistol_Nelson,
        Pistol_Nelson_Decorated,
        Sword_Peg_Leg,
        Sword_Peg_Leg_Decorated,
        Musket_Barbary,
        Musket_Barbary_Decorated,
        Unarmed_Drunk,
        Sword_Battle_Axe,
        Sword_War_Hammer
    };

    const char* GetWeaponTypeName(uint16_t value) {
        switch (static_cast<WeaponType>(value)) {
            case WeaponType::Unarmed: return "Unarmed";
            case WeaponType::CannonRamRodRammingObject: return "CannonRamRodRammingObject";
            case WeaponType::Sword_1796_Epee: return "Sword_1796_Epee";
            case WeaponType::Sword_1805_Epee: return "Sword_1805_Epee";
            case WeaponType::Sword_Sabre_Officier_Infanterie: return "Sword_Sabre_Officier_Infanterie";
            case WeaponType::BroadsideCannon: return "BroadsideCannon";
            case WeaponType::CarronadeCannon: return "CarronadeCannon";
            case WeaponType::SwivelCannon: return "SwivelCannon";
            case WeaponType::CoastalCannon: return "CoastalCannon";
            case WeaponType::Cannon24Pounder: return "Cannon24Pounder";
            case WeaponType::MortarCannon: return "MortarCannon";
            case WeaponType::BuckShotHowitzer: return "BuckShotHowitzer";
            case WeaponType::FieldHowitzer: return "FieldHowitzer";
            case WeaponType::Cannon4Pounder: return "Cannon4Pounder";
            case WeaponType::BearingFlag: return "BearingFlag";
            case WeaponType::FieldCannon9Pounder: return "FieldCannon9Pounder";
            case WeaponType::Cannon4PounderWheelCarriage: return "Cannon4PounderWheelCarriage";
            case WeaponType::Carronade_RotatingCannonCarriage: return "Carronade_RotatingCannonCarriage";
            case WeaponType::CannonRamRod: return "CannonRamRod";
            case WeaponType::CannonRoundShot: return "CannonRoundShot";
            case WeaponType::BuckShot: return "BuckShot";
            case WeaponType::RocketArtillery: return "RocketArtillery";
            case WeaponType::DeconstructingHammer: return "DeconstructingHammer";
            case WeaponType::ConstructingHammer: return "ConstructingHammer";
            case WeaponType::DiggingSpade: return "DiggingSpade";
            case WeaponType::BuckShotSmall: return "BuckShotSmall";
            case WeaponType::BuckShotLarge: return "BuckShotLarge";
            case WeaponType::Depreacted_Musket_SeaServiceBrownBess: return "Depreacted_Musket_SeaServiceBrownBess";
            case WeaponType::Deprecated_Musket_CharlevilleMarinePatternANIX: return "Deprecated_Musket_CharlevilleMarinePatternANIX";
            case WeaponType::Deprecated_Musket_IndiaPatternBrownBess: return "Deprecated_Musket_IndiaPatternBrownBess";
            case WeaponType::Musket_Charleville_Musket_IXXIII_Standard_Year: return "Musket_Charleville_Musket_IXXIII_Standard_Year";
            case WeaponType::Musket_Land_Pattern_Musket_Brown_Bess: return "Musket_Land_Pattern_Musket_Brown_Bess";
            case WeaponType::Deprecated_Musket_CharlevilleGuardPattern: return "Deprecated_Musket_CharlevilleGuardPattern";
            case WeaponType::Deprecated_Musket_NewLandLightInfantryPatternBrownBess: return "Deprecated_Musket_NewLandLightInfantryPatternBrownBess";
            case WeaponType::Deprecated_Musket_CharlevilleDragoonPattern: return "Deprecated_Musket_CharlevilleDragoonPattern";
            case WeaponType::Carbine_Elliot_Carbine: return "Carbine_Elliot_Carbine";
            case WeaponType::Carbine_Charleville_Cavalry_Carbine_IXXIII: return "Carbine_Charleville_Cavalry_Carbine_IXXIII";
            case WeaponType::Rifle_Baker_Rifle_Pattern_1800: return "Rifle_Baker_Rifle_Pattern_1800";
            case WeaponType::Depreacted_Rifle_CharlevilleDragoonPattern_NoBayonet: return "Depreacted_Rifle_CharlevilleDragoonPattern_NoBayonet";
            case WeaponType::Deprecated_Pistol_SeaService: return "Deprecated_Pistol_SeaService";
            case WeaponType::Pistol_French_Pistol_XIII: return "Pistol_French_Pistol_XIII";
            case WeaponType::Pistol_Land_Pattern_Pistol: return "Pistol_Land_Pattern_Pistol";
            case WeaponType::Blunderbuss_French_Blunderbuss: return "Blunderbuss_French_Blunderbuss";
            case WeaponType::Deprecated_Blunderbuss_Variation2: return "Deprecated_Blunderbuss_Variation2";
            case WeaponType::Axe2H_Variation1: return "Axe2H_Variation1";
            case WeaponType::Axe2H_British_2Handed_Axe: return "Axe2H_British_2Handed_Axe";
            case WeaponType::Axe1H_Carpenter_Adze: return "Axe1H_Carpenter_Adze";
            case WeaponType::Axe1H_Carpenter_Axe: return "Axe1H_Carpenter_Axe";
            case WeaponType::Deprecated_Sword_Pattern1803FlankOfficer: return "Deprecated_Sword_Pattern1803FlankOfficer";
            case WeaponType::Sword_An_XI_Light_Sabre: return "Sword_An_XI_Light_Sabre";
            case WeaponType::Sword_1715_Pattern_Sabre: return "Sword_1715_Pattern_Sabre";
            case WeaponType::Deprecated_Sabre_AnXILight: return "Deprecated_Sabre_AnXILight";
            case WeaponType::Deprecated_Sabre_1796PatternLight: return "Deprecated_Sabre_1796PatternLight";
            case WeaponType::Sword_Baker_Rifle_Bayonet: return "Sword_Baker_Rifle_Bayonet";
            case WeaponType::Sword_Sabre_Briquet: return "Sword_Sabre_Briquet";
            case WeaponType::Sword_British_Hanger: return "Sword_British_Hanger";
            case WeaponType::Sword_French_Hanger: return "Sword_French_Hanger";
            case WeaponType::Sword_1804_Pattern_Naval_Cutlass: return "Sword_1804_Pattern_Naval_Cutlass";
            case WeaponType::Sword_Year_IX_Boarding_Sabre: return "Sword_Year_IX_Boarding_Sabre";
            case WeaponType::Knife_British_Dagger: return "Knife_British_Dagger";
            case WeaponType::Knife_French_Dagger: return "Knife_French_Dagger";
            case WeaponType::Knife_British_Knife: return "Knife_British_Knife";
            case WeaponType::Knife_French_Knife: return "Knife_French_Knife";
            case WeaponType::Spontoon_British_Boarding_Pike: return "Spontoon_British_Boarding_Pike";
            case WeaponType::Spontoon_French_Boarding_Pike: return "Spontoon_French_Boarding_Pike";
            case WeaponType::ExplosiveBarrel: return "ExplosiveBarrel";
            case WeaponType::Sword_Pattern_1803_Flank_Officer_Sabre: return "Sword_Pattern_1803_Flank_Officer_Sabre";
            case WeaponType::Sword_1796_Pattern_Light_Sabre: return "Sword_1796_Pattern_Light_Sabre";
            case WeaponType::Sword_Russian_Epee_Sword: return "Sword_Russian_Epee_Sword";
            case WeaponType::Deprecated_Sword_Russian_Epee_Officer_Sword: return "Deprecated_Sword_Russian_Epee_Officer_Sword";
            case WeaponType::Sword_Russian_Hussar_Sword: return "Sword_Russian_Hussar_Sword";
            case WeaponType::Sword_Russian_Cuirassier_Sword: return "Sword_Russian_Cuirassier_Sword";
            case WeaponType::Sword_Russian_Rifle_Sword: return "Sword_Russian_Rifle_Sword";
            case WeaponType::Pistol_Russian_1809_Pistol: return "Pistol_Russian_1809_Pistol";
            case WeaponType::Musket_Russian_Musket_Pattern_1808: return "Musket_Russian_Musket_Pattern_1808";
            case WeaponType::Deprecated_Musket_Russian_1808_Guard: return "Deprecated_Musket_Russian_1808_Guard";
            case WeaponType::Deprecated_Musket_Russian_1808_Light: return "Deprecated_Musket_Russian_1808_Light";
            case WeaponType::Rifle_Russian_Rifle_Pattern_1806: return "Rifle_Russian_Rifle_Pattern_1806";
            case WeaponType::Carbine_Russian_Cuirassier_Carbine: return "Carbine_Russian_Cuirassier_Carbine";
            case WeaponType::Sword_Claymore: return "Sword_Claymore";
            case WeaponType::Frontlines_MachineGun_Allied: return "Frontlines_MachineGun_Allied";
            case WeaponType::Frontlines_MachineGun_Central: return "Frontlines_MachineGun_Central";
            case WeaponType::Frontlines_LeeEnfield_MKIII: return "Frontlines_LeeEnfield_MKIII";
            case WeaponType::Frontlines_Mauser_Gewehr_98: return "Frontlines_Mauser_Gewehr_98";
            case WeaponType::Frontlines_M1903_Springfield: return "Frontlines_M1903_Springfield";
            case WeaponType::Frontlines_Lebel_Model_1886: return "Frontlines_Lebel_Model_1886";
            case WeaponType::Frontlines_Webley_Revolver_MKIV: return "Frontlines_Webley_Revolver_MKIV";
            case WeaponType::Frontlines_Modele_1892_Revolver: return "Frontlines_Modele_1892_Revolver";
            case WeaponType::Frontlines_M1917_Revolver: return "Frontlines_M1917_Revolver";
            case WeaponType::Frontlines_Mauser_C78_Revolver: return "Frontlines_Mauser_C78_Revolver";
            case WeaponType::Frontlines_Pattern_1897_Officer_Sword: return "Frontlines_Pattern_1897_Officer_Sword";
            case WeaponType::Frontlines_M1882_Sabre_d_officier: return "Frontlines_M1882_Sabre_d_officier";
            case WeaponType::Frontlines_Model_1902_Army_Officers_Sword: return "Frontlines_Model_1902_Army_Officers_Sword";
            case WeaponType::Frontlines_M1889_infantrie_saebel: return "Frontlines_M1889_infantrie_saebel";
            case WeaponType::Frontlines_British_Trench_Club: return "Frontlines_British_Trench_Club";
            case WeaponType::Frontlines_French_Trench_Club: return "Frontlines_French_Trench_Club";
            case WeaponType::Frontlines_German_Trench_Club: return "Frontlines_German_Trench_Club";
            case WeaponType::Frontlines_P1907_Trench_Knife: return "Frontlines_P1907_Trench_Knife";
            case WeaponType::Frontlines_M1916_Vengeur_Knife: return "Frontlines_M1916_Vengeur_Knife";
            case WeaponType::Frontlines_1918_Trench_Knife: return "Frontlines_1918_Trench_Knife";
            case WeaponType::Frontlines_German_Boker_Knife: return "Frontlines_German_Boker_Knife";
            case WeaponType::Frontlines_Allied_Shovel: return "Frontlines_Allied_Shovel";
            case WeaponType::Frontlines_CentralPower_Shovel: return "Frontlines_CentralPower_Shovel";
            case WeaponType::Frontlines_LeeEnfield_MKIII_No_Bayonet: return "Frontlines_LeeEnfield_MKIII_No_Bayonet";
            case WeaponType::Frontlines_Mauser_Gewehr_98_No_Bayonet: return "Frontlines_Mauser_Gewehr_98_No_Bayonet";
            case WeaponType::Frontlines_M1903_Springfield_No_Bayonet: return "Frontlines_M1903_Springfield_No_Bayonet";
            case WeaponType::Frontlines_Lebel_Model_1886_No_Bayonet: return "Frontlines_Lebel_Model_1886_No_Bayonet";
            case WeaponType::Frontlines_MillsGrenade_MKII: return "Frontlines_MillsGrenade_MKII";
            case WeaponType::Frontlines_Stielhandgranate_1916: return "Frontlines_Stielhandgranate_1916";
            case WeaponType::Frontlines_No37_MKI: return "Frontlines_No37_MKI";
            case WeaponType::Frontlines_HandNebelBombe: return "Frontlines_HandNebelBombe";
            case WeaponType::Frontlines_Howitzer: return "Frontlines_Howitzer";
            case WeaponType::Frontlines_HeavyMortar: return "Frontlines_HeavyMortar";
            case WeaponType::Frontlines_TNT: return "Frontlines_TNT";
            case WeaponType::Frontlines_HeavyHowitzer: return "Frontlines_HeavyHowitzer";
            case WeaponType::Frontlines_ShipTurret: return "Frontlines_ShipTurret";
            case WeaponType::Unused_Frontlines_GreaseGun: return "Unused_Frontlines_GreaseGun";
            case WeaponType::Unused_Frontlines_TrenchGun: return "Unused_Frontlines_TrenchGun";
            case WeaponType::Frontlines_3InchDeckGun: return "Frontlines_3InchDeckGun";
            case WeaponType::Frontlines_CoastGun: return "Frontlines_CoastGun";
            case WeaponType::Unarmed_Surrender: return "Unarmed_Surrender";
            case WeaponType::Musket_Shoulder_Arms_Gesture: return "Musket_Shoulder_Arms_Gesture";
            case WeaponType::Deprecated_Musket_SeaServiceBrownbess_NoBayonet: return "Deprecated_Musket_SeaServiceBrownbess_NoBayonet";
            case WeaponType::Deprecated_Musket_LandLightInfantryPatternBrownBess_NoBayonet: return "Deprecated_Musket_LandLightInfantryPatternBrownBess_NoBayonet";
            case WeaponType::Deprecated_Musket_1808PatternMusket_NoBayonet: return "Deprecated_Musket_1808PatternMusket_NoBayonet";
            case WeaponType::Deprecated_Musket_IndiaPatternBrownBess_NoBayonet: return "Deprecated_Musket_IndiaPatternBrownBess_NoBayonet";
            case WeaponType::Deprecated_Musket_CharlevilleMusketIX_XIII_NoBayonet: return "Deprecated_Musket_CharlevilleMusketIX_XIII_NoBayonet";
            case WeaponType::Deprecated_Musket_1808PatternMusket_LightVariant_NoBayonet: return "Deprecated_Musket_1808PatternMusket_LightVariant_NoBayonet";
            case WeaponType::Deprecated_Musket_CharlevilleMusketMarinePatternAnIX_NoBayonet: return "Deprecated_Musket_CharlevilleMusketMarinePatternAnIX_NoBayonet";
            case WeaponType::Deprecated_Musket_CharlevilleMusketDragoonPattern_NoBayonet: return "Deprecated_Musket_CharlevilleMusketDragoonPattern_NoBayonet";
            case WeaponType::Spontoon_Russian_Spontoon: return "Spontoon_Russian_Spontoon";
            case WeaponType::Spontoon_Prussian_Spontoon: return "Spontoon_Prussian_Spontoon";
            case WeaponType::Spontoon_French_Spontoon: return "Spontoon_French_Spontoon";
            case WeaponType::Spontoon_British_Spontoon: return "Spontoon_British_Spontoon";
            case WeaponType::BroadsideCannon_Small: return "BroadsideCannon_Small";
            case WeaponType::Unarmed_Zombie: return "Unarmed_Zombie";
            case WeaponType::ChristmasConfettiBuckshot: return "ChristmasConfettiBuckshot";
            case WeaponType::ChristmasConfettiBuckshotSmall: return "ChristmasConfettiBuckshotSmall";
            case WeaponType::Napoleon_Waistcoat: return "Napoleon_Waistcoat";
            case WeaponType::Napoleon_BehindBack: return "Napoleon_BehindBack";
            case WeaponType::Napoleon_Charge: return "Napoleon_Charge";
            case WeaponType::Musket_Charleville_Musket_Dragoon_Pattern_Rusty: return "Musket_Charleville_Musket_Dragoon_Pattern_Rusty";
            case WeaponType::Sword_Scimitar: return "Sword_Scimitar";
            case WeaponType::Sword_Napoleons_Sword: return "Sword_Napoleons_Sword";
            case WeaponType::Sword_Yatagan_Sword: return "Sword_Yatagan_Sword";
            case WeaponType::Sword_Frying_Pan: return "Sword_Frying_Pan";
            case WeaponType::Axe2H_Sapper_2Handed_Axe: return "Axe2H_Sapper_2Handed_Axe";
            case WeaponType::Spontoon_Halberd: return "Spontoon_Halberd";
            case WeaponType::Musket_Charleville_Musket_Guard_Pattern_Decorated: return "Musket_Charleville_Musket_Guard_Pattern_Decorated";
            case WeaponType::Musket_Land_Light_Infantry_Pattern_Musket_Brown_Bess_Rusty: return "Musket_Land_Light_Infantry_Pattern_Musket_Brown_Bess_Rusty";
            case WeaponType::Musket_Sea_Service_Musket_Brown_Bess_Decorated: return "Musket_Sea_Service_Musket_Brown_Bess_Decorated";
            case WeaponType::Musket_Russian_Musket_Pattern_1808_Light_Pattern_Rusty: return "Musket_Russian_Musket_Pattern_1808_Light_Pattern_Rusty";
            case WeaponType::Musket_Russian_Musket_Pattern_1808_Guard_Pattern_Decorated: return "Musket_Russian_Musket_Pattern_1808_Guard_Pattern_Decorated";
            case WeaponType::Musket_Spanish_Escopeta_Musket: return "Musket_Spanish_Escopeta_Musket";
            case WeaponType::Musket_Spanish_Escopeta_Musket_Decorated: return "Musket_Spanish_Escopeta_Musket_Decorated";
            case WeaponType::Musket_Ottoman_Tufenk_Musket: return "Musket_Ottoman_Tufenk_Musket";
            case WeaponType::Musket_Ottoman_Tufenk_Musket_Decorated: return "Musket_Ottoman_Tufenk_Musket_Decorated";
            case WeaponType::Musket_Boutet_Musket: return "Musket_Boutet_Musket";
            case WeaponType::Musket_Boutet_Musket_Decorated: return "Musket_Boutet_Musket_Decorated";
            case WeaponType::Rifle_Baker_Rifle_Pattern_1800_Rusty: return "Rifle_Baker_Rifle_Pattern_1800_Rusty";
            case WeaponType::Rifle_Baker_Rifle_Pattern_1800_Decorated: return "Rifle_Baker_Rifle_Pattern_1800_Decorated";
            case WeaponType::Rifle_Russian_Rifle_Pattern_1806_Rusty: return "Rifle_Russian_Rifle_Pattern_1806_Rusty";
            case WeaponType::Rifle_Russian_Rifle_Pattern_1806_Decorated: return "Rifle_Russian_Rifle_Pattern_1806_Decorated";
            case WeaponType::Rifle_Sporting_Rifle: return "Rifle_Sporting_Rifle";
            case WeaponType::Rifle_Sporting_Rifle_Decorated: return "Rifle_Sporting_Rifle_Decorated";
            case WeaponType::Carbine_Charleville_Cavalry_Carbine_IXXIII_Rusty: return "Carbine_Charleville_Cavalry_Carbine_IXXIII_Rusty";
            case WeaponType::Carbine_Charleville_Cavalry_Carbine_IXXIII_Decorated: return "Carbine_Charleville_Cavalry_Carbine_IXXIII_Decorated";
            case WeaponType::Carbine_Elliot_Carbine_Rusty: return "Carbine_Elliot_Carbine_Rusty";
            case WeaponType::Carbine_Elliot_Carbine_Decorated: return "Carbine_Elliot_Carbine_Decorated";
            case WeaponType::Carbine_Russian_Cuirassier_Carbine_Rusty: return "Carbine_Russian_Cuirassier_Carbine_Rusty";
            case WeaponType::Carbine_Russian_Cuirassier_Carbine_Decorated: return "Carbine_Russian_Cuirassier_Carbine_Decorated";
            case WeaponType::Carbine_Sporting_Carbine: return "Carbine_Sporting_Carbine";
            case WeaponType::Carbine_Sporting_Carbine_Decorated: return "Carbine_Sporting_Carbine_Decorated";
            case WeaponType::Pistol_Land_Pattern_Pistol_Rusty: return "Pistol_Land_Pattern_Pistol_Rusty";
            case WeaponType::Pistol_Sea_Service_Pistol_Decorated: return "Pistol_Sea_Service_Pistol_Decorated";
            case WeaponType::Pistol_French_Pistol_XIII_Rusty: return "Pistol_French_Pistol_XIII_Rusty";
            case WeaponType::Pistol_French_Pistol_XIII_Decorated: return "Pistol_French_Pistol_XIII_Decorated";
            case WeaponType::Pistol_Russian_1809_Pistol_Rusty: return "Pistol_Russian_1809_Pistol_Rusty";
            case WeaponType::Pistol_Russian_1809_Pistol_Decorated: return "Pistol_Russian_1809_Pistol_Decorated";
            case WeaponType::Pistol_Le_Page_Pistol: return "Pistol_Le_Page_Pistol";
            case WeaponType::Pistol_Le_Page_Pistol_Decorated: return "Pistol_Le_Page_Pistol_Decorated";
            case WeaponType::Blunderbuss_British_Blunderbuss_Rusty: return "Blunderbuss_British_Blunderbuss_Rusty";
            case WeaponType::Blunderbuss_French_Blunderbuss_Decorated: return "Blunderbuss_French_Blunderbuss_Decorated";
            case WeaponType::Blunderbuss_Nock_Gun_Blunderbuss: return "Blunderbuss_Nock_Gun_Blunderbuss";
            case WeaponType::Blunderbuss_Nock_Gun_Blunderbuss_Decorated: return "Blunderbuss_Nock_Gun_Blunderbuss_Decorated";
            case WeaponType::Musket_Iroquois_Musket: return "Musket_Iroquois_Musket";
            case WeaponType::Sword_Frying_Pan_Gold: return "Sword_Frying_Pan_Gold";
            case WeaponType::Deprecated_Frontlines_Frying_Pan: return "Deprecated_Frontlines_Frying_Pan";
            case WeaponType::Deprecated_Frontlines_Frying_Pan_Gold: return "Deprecated_Frontlines_Frying_Pan_Gold";
            case WeaponType::Frontlines_Lebel_Model_1886_Decorated: return "Frontlines_Lebel_Model_1886_Decorated";
            case WeaponType::Frontlines_Lebel_Model_1886_Rusty: return "Frontlines_Lebel_Model_1886_Rusty";
            case WeaponType::Frontlines_LeeEnfield_MKIII_Decorated: return "Frontlines_LeeEnfield_MKIII_Decorated";
            case WeaponType::Frontlines_LeeEnfield_MKIII_Rusty: return "Frontlines_LeeEnfield_MKIII_Rusty";
            case WeaponType::Frontlines_M1903_Springfield_Decorated: return "Frontlines_M1903_Springfield_Decorated";
            case WeaponType::Frontlines_M1903_Springfield_Rusty: return "Frontlines_M1903_Springfield_Rusty";
            case WeaponType::Frontlines_Mauser_Gewehr_98_Decorated: return "Frontlines_Mauser_Gewehr_98_Decorated";
            case WeaponType::Frontlines_Mauser_Gewehr_98_Rusty: return "Frontlines_Mauser_Gewehr_98_Rusty";
            case WeaponType::Frontlines_M1897_Trench_Gun: return "Frontlines_M1897_Trench_Gun";
            case WeaponType::Frontlines_M1897_Trench_Gun_Warthorn: return "Frontlines_M1897_Trench_Gun_Warthorn";
            case WeaponType::Frontlines_M1897_Trench_Gun_Decorated: return "Frontlines_M1897_Trench_Gun_Decorated";
            case WeaponType::Sword_Whiskey_Bottle: return "Sword_Whiskey_Bottle";
            case WeaponType::Sword_Whiskey_Bottle_Broken: return "Sword_Whiskey_Bottle_Broken";
            case WeaponType::Sword_Whiskey_Bottle_Decorated: return "Sword_Whiskey_Bottle_Decorated";
            case WeaponType::Sword_Rum_Bottle: return "Sword_Rum_Bottle";
            case WeaponType::Sword_Rum_Bottle_Broken: return "Sword_Rum_Bottle_Broken";
            case WeaponType::Sword_Rum_Bottle_Decorated: return "Sword_Rum_Bottle_Decorated";
            case WeaponType::Musket_Tartan_Wrapped: return "Musket_Tartan_Wrapped";
            case WeaponType::Sword_Nelson: return "Sword_Nelson";
            case WeaponType::Pistol_Nelson: return "Pistol_Nelson";
            case WeaponType::Pistol_Nelson_Decorated: return "Pistol_Nelson_Decorated";
            case WeaponType::Sword_Peg_Leg: return "Sword_Peg_Leg";
            case WeaponType::Sword_Peg_Leg_Decorated: return "Sword_Peg_Leg_Decorated";
            case WeaponType::Musket_Barbary: return "Musket_Barbary";
            case WeaponType::Musket_Barbary_Decorated: return "Musket_Barbary_Decorated";
            case WeaponType::Unarmed_Drunk: return "Unarmed_Drunk";
            case WeaponType::Sword_Battle_Axe: return "Sword_Battle_Axe";
            case WeaponType::Sword_War_Hammer: return "Sword_War_Hammer";
            default: return "Unknown";
        }
    }
}
