using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using Metin2.Runtime;
using Metin2.Runtime.Network;
using UnityEditor;
using UnityEditor.Animations;
using UnityEngine;

namespace Metin2.Editor
{
    /// <summary>Validates every mount race Auriga can equip, not only the legacy pony fixture.</summary>
    public static class Metin2AurigaMountAssetValidator
    {
        public static void RunFromCommandLine() => Run(true);

        [MenuItem("Metin2/Validate/Auriga Mount Assets")]
        public static void RunInEditor() => Run(false);

        private static void Run(bool exitWhenFinished)
        {
            try
            {
                Metin2ItemCatalog items = AssetDatabase.LoadAssetAtPath<Metin2ItemCatalog>(
                    "Assets/Metin2/GeneratedData/Metin2ItemCatalog.asset");
                Metin2RaceCatalog races = AssetDatabase.LoadAssetAtPath<Metin2RaceCatalog>(
                    "Assets/Metin2/GeneratedData/Metin2RaceCatalog.asset");
                Metin2RaceDataCatalog data = AssetDatabase.LoadAssetAtPath<Metin2RaceDataCatalog>(
                    "Assets/Metin2/GeneratedData/Metin2RaceDataCatalog.asset");
                if (items == null || races == null || data == null)
                    throw new InvalidOperationException("Mount validator catalogs are missing");

                var uniqueRaces = new HashSet<uint>();
                var failures = new List<string>();
                int itemsChecked = 0, prefabs = 0, waitRun = 0, saddles = 0;
                int renderersChecked = 0, materialSlots = 0, texturedSlots = 0, fallbackRun = 0;
                int loopClips = 0;
                float maximumSaddleOffset = 0f;
                var speeds = new List<float>();
                var report = new StringBuilder("race,item,prefab,renderers,material_slots,textured_slots,run_state,run_mps\n");
                foreach (uint itemVnum in Metin2AuxiliaryInventoryPanel.MountAllowedItemVnums)
                {
                    itemsChecked++;
                    if (!items.TryGet(itemVnum, out Metin2ItemCatalog.Entry item))
                    { failures.Add($"item:{itemVnum}:missing-proto"); continue; }
                    uint race = (uint)Mathf.Max(0, item.GetValue(1));
                    if (race == 0)
                    { failures.Add($"item:{itemVnum}:zero-race"); continue; }
                    if (!uniqueRaces.Add(race)) continue;
                    races.TryGet(race,out GameObject prefab);
                    if(prefab==null)
                    {
                        var published=AssetDatabase.LoadAssetAtPath<Metin2RacePrefabAsset>("Assets/Metin2/AddressableContent/Races/Prefabs/"+race+".asset");
                        if(published!=null&&published.prefab!=null&&published.prefab.GetComponentInChildren<TeraCompanionPresentation>(true)!=null)prefab=published.prefab;
                    }
                    if (prefab == null)
                    { failures.Add($"race:{race}:missing-prefab"); continue; }
                    bool tera=prefab.GetComponentInChildren<TeraCompanionPresentation>(true)!=null;
                    prefabs++;
                    Renderer[] renderers = prefab.GetComponentsInChildren<Renderer>(true)
                        .Where(renderer => !(renderer is ParticleSystemRenderer)).ToArray();
                    if (renderers.Length == 0)
                        failures.Add($"race:{race}:no-renderer");
                    int raceSlots = 0, raceTextured = 0;
                    foreach (Renderer renderer in renderers)
                    {
                        renderersChecked++;
                        foreach (Material material in renderer.sharedMaterials)
                        {
                            materialSlots++;
                            raceSlots++;
                            if (material != null && material.mainTexture != null)
                            {
                                texturedSlots++;
                                raceTextured++;
                            }
                            else if(!tera||material==null||material.shader==null||material.shader.name.Contains("InternalError"))
                                failures.Add($"race:{race}:untextured:{renderer.name}:{material?.name ?? "<null>"}");
                        }
                    }

                    Transform saddle = prefab.GetComponentsInChildren<Transform>(true)
                        .FirstOrDefault(node => node.name.Equals("saddle", StringComparison.OrdinalIgnoreCase));
                    if (saddle == null) failures.Add($"race:{race}:missing-saddle");
                    else
                    {
                        saddles++;
                        maximumSaddleOffset = Mathf.Max(maximumSaddleOffset,
                            new Vector2(saddle.position.x, saddle.position.z).magnitude);
                    }

                    data.TryGet(race,out Metin2RaceDataCatalog.Entry raceData);
                    if(raceData==null&&tera)raceData=AssetDatabase.LoadAssetAtPath<Metin2RaceDataAsset>("Assets/Metin2/AddressableContent/Races/Data/"+race+".asset")?.entry;
                    if (raceData == null)
                    { failures.Add($"race:{race}:missing-race-data"); continue; }
                    string wait = raceData.StateFor(Metin2RaceDataCatalog.MotionWait);
                    string run = raceData.StateFor(Metin2RaceDataCatalog.MotionRun) ??
                                 raceData.StateFor(Metin2RaceDataCatalog.MotionWalk);
                    Metin2MotionDefinition runDefinition = raceData.DefinitionFor(Metin2RaceDataCatalog.MotionRun) ??
                                                           raceData.DefinitionFor(Metin2RaceDataCatalog.MotionWalk);
                    Metin2MotionDefinition waitDefinition = raceData.DefinitionFor(Metin2RaceDataCatalog.MotionWait);
                    Animator animator = prefab.GetComponentInChildren<Animator>(true);
                    AnimatorController controller = animator != null
                        ? animator.runtimeAnimatorController as AnimatorController : null;
                    // A handful of multi-descriptor community mounts intentionally share one
                    // controller while their catalog motion-definition is null. The Animator state
                    // is the runtime source in that case, so validate its actual imported clip.
                    AnimationClip waitClip = waitDefinition?.clip ?? StateClip(controller, wait);
                    AnimationClip runClip = runDefinition?.clip ?? StateClip(controller, run);
                    if (!IsImportedLoop(waitClip))
                        failures.Add($"race:{race}:wait-not-looping:{wait ?? "<null>"}");
                    else loopClips++;
                    if (!IsImportedLoop(runClip))
                        failures.Add($"race:{race}:run-not-looping:{run ?? "<null>"}");
                    else loopClips++;
                    float runSpeed = 0f;
                    if (!Metin2CharacterMotionSet.TryGetDefinitionTravelSpeed(runDefinition, out runSpeed))
                        fallbackRun++;
                    else
                        speeds.Add(runSpeed);
                    if (controller == null || string.IsNullOrEmpty(wait) || string.IsNullOrEmpty(run) ||
                        !HasState(controller, wait) || !HasState(controller, run))
                    {
                        failures.Add($"race:{race}:wait-run:{wait ?? "<null>"}|{run ?? "<null>"}");
                        continue;
                    }
                    waitRun++;
                    report.Append(race).Append(',').Append(itemVnum).Append(',')
                        .Append(prefab.name).Append(',').Append(renderers.Length).Append(',')
                        .Append(raceSlots).Append(',').Append(raceTextured).Append(',')
                        .Append(run).Append(',').Append(runSpeed.ToString("0.###", System.Globalization.CultureInfo.InvariantCulture))
                        .Append('\n');
                }

                string reportPath = "Migration/Converted/auriga-mount-presentation-report.csv";
                File.WriteAllText(reportPath, report.ToString(), new UTF8Encoding(false));

                if (failures.Count != 0)
                    throw new InvalidOperationException("Auriga mount asset gaps: " + string.Join(",", failures));
                speeds.Sort();
                float median = speeds.Count == 0 ? 0f : speeds[speeds.Count / 2];
                Debug.Log($"METIN2_AURIGA_MOUNT_ASSETS=SUCCESS items={itemsChecked} races={uniqueRaces.Count} " +
                          $"prefabs={prefabs} saddles={saddles} waitRun={waitRun} renderers={renderersChecked} " +
                          $"textures={texturedSlots}/{materialSlots} runMps=" +
                          $"{(speeds.Count > 0 ? speeds[0] : 0f):0.###}/{median:0.###}/" +
                          $"{(speeds.Count > 0 ? speeds[speeds.Count - 1] : 0f):0.###} fallbackRun={fallbackRun} " +
                          $"loopClips={loopClips}/{uniqueRaces.Count * 2} maxSaddleOffset={maximumSaddleOffset:0.###}");
                if (exitWhenFinished) EditorApplication.Exit(0);
            }
            catch (Exception exception)
            {
                Debug.LogException(exception);
                Debug.LogError("METIN2_AURIGA_MOUNT_ASSETS=FAILED");
                if (exitWhenFinished) EditorApplication.Exit(1);
                else throw;
            }
        }

        private static bool HasState(AnimatorController controller, string state)
            => controller.layers.Any(layer => layer.stateMachine.states.Any(child =>
                child.state != null && child.state.name.Equals(state, StringComparison.Ordinal)));

        private static AnimationClip StateClip(AnimatorController controller, string state)
        {
            if (controller == null || string.IsNullOrEmpty(state)) return null;
            foreach (AnimatorControllerLayer layer in controller.layers)
                foreach (ChildAnimatorState child in layer.stateMachine.states)
                    if (child.state != null && child.state.name.Equals(state, StringComparison.Ordinal))
                        return child.state.motion as AnimationClip;
            return null;
        }

        private static bool IsImportedLoop(AnimationClip clip)
        {
            if (clip == null) return false;
            ModelImporter importer = AssetImporter.GetAtPath(AssetDatabase.GetAssetPath(clip)) as ModelImporter;
            if (importer == null) return clip.isLooping;
            ModelImporterClipAnimation[] clips = importer.clipAnimations;
            if (clips == null || clips.Length == 0) clips = importer.defaultClipAnimations;
            string wanted = ClipSuffix(clip.name);
            return clips != null && clips.Any(item =>
                ClipSuffix(item.name).Equals(wanted, StringComparison.OrdinalIgnoreCase) && item.loopTime);
        }

        private static string ClipSuffix(string value)
        {
            if (string.IsNullOrEmpty(value)) return string.Empty;
            int separator = value.LastIndexOf('|');
            return separator >= 0 ? value.Substring(separator + 1) : value;
        }
    }
}
