-- Generated from bonus_registry.json. Fresh draft catalog only; duplicate IDs fail.
-- Bounds are individual structural validation, not aggregated gameplay caps.
START TRANSACTION;
INSERT INTO protos.bonus_definition
    (bonus_id,code,display_key,unit_kind,target_kind,stage_kind,channel_filter,action_filter,mode_filter,
     family_filter,rank_filter,element_filter,stack_policy,aggregate_cap,allowed_sources,minimum_value,maximum_value) VALUES
    (1,'physical_attack.flat','bonus.physical_attack.flat',1,1,1,2,1,1,0,0,0,1,1,3,-1000000000000000,1000000000000000),
    (2,'magical_attack.flat','bonus.magical_attack.flat',1,2,1,3,1,1,0,0,0,1,1,3,-1000000000000000,1000000000000000),
    (3,'physical_defense.flat','bonus.physical_defense.flat',1,3,1,2,1,1,0,0,0,1,1,3,-1000000000000000,1000000000000000),
    (4,'magical_defense.flat','bonus.magical_defense.flat',1,4,1,3,1,1,0,0,0,1,1,3,-1000000000000000,1000000000000000),
    (5,'physical_attack.percent','bonus.physical_attack.percent',2,1,2,2,1,1,0,0,0,1,2,3,-100000,100000),
    (6,'magical_attack.percent','bonus.magical_attack.percent',2,2,2,3,1,1,0,0,0,1,2,3,-100000,100000),
    (7,'normal.outgoing','bonus.normal.outgoing',2,5,3,1,2,1,0,0,0,1,3,3,-100000,100000),
    (8,'skill.outgoing','bonus.skill.outgoing',2,5,3,1,3,1,0,0,0,1,3,3,-100000,100000),
    (9,'pve.outgoing','bonus.pve.outgoing',2,5,4,1,1,2,0,0,0,1,3,3,-100000,100000),
    (10,'pvp.outgoing','bonus.pvp.outgoing',2,5,4,1,1,3,0,0,0,1,3,3,-100000,100000),
    (11,'normal.incoming','bonus.normal.incoming',2,6,8,1,2,1,0,0,0,1,4,3,-100000,100000),
    (12,'skill.incoming','bonus.skill.incoming',2,6,8,1,3,1,0,0,0,1,4,3,-100000,100000),
    (13,'pve.incoming','bonus.pve.incoming',2,6,9,1,1,2,0,0,0,1,4,3,-100000,100000),
    (14,'pvp.incoming','bonus.pvp.incoming',2,6,9,1,1,3,0,0,0,1,4,3,-100000,100000),
    (15,'physical.penetration.flat','bonus.physical.penetration.flat',1,7,5,2,1,1,0,0,0,1,5,3,0,1000000000000000),
    (16,'magical.penetration.flat','bonus.magical.penetration.flat',1,7,5,3,1,1,0,0,0,1,5,3,0,1000000000000000),
    (17,'physical.penetration.percent','bonus.physical.penetration.percent',2,8,5,2,1,1,0,0,0,1,6,3,0,10000),
    (18,'magical.penetration.percent','bonus.magical.penetration.percent',2,8,5,3,1,1,0,0,0,1,6,3,0,10000),
    (19,'physical.resistance','bonus.physical.resistance',2,9,6,2,1,1,0,0,0,1,7,3,-100000,100000),
    (20,'magical.resistance','bonus.magical.resistance',2,9,6,3,1,1,0,0,0,1,7,3,-100000,100000),
    (21,'critical.chance','bonus.critical.chance',2,10,7,1,1,1,0,0,0,1,8,3,-100000,100000),
    (22,'critical.resistance','bonus.critical.resistance',2,11,7,1,1,1,0,0,0,1,9,3,-100000,100000),
    (23,'critical.multiplier_extra','bonus.critical.multiplier_extra',2,12,7,1,1,1,0,0,0,1,10,3,-100000,100000),
    (24,'block.reduction','bonus.block.reduction',2,13,10,2,1,1,0,0,0,1,11,3,0,10000),
    (25,'accuracy.evasion','bonus.accuracy.evasion',2,14,11,1,1,1,0,0,0,1,12,3,-100000,100000);
COMMIT;
