using System;
using System.Collections.Generic;
using System.Linq;
using UnityEditor.AddressableAssets;
using UnityEditor.Build;
using UnityEngine.AddressableAssets.ResourceLocators;

namespace Metin2.Editor.Patching
{
    internal static class Metin2CatalogAddressValidator
    {
        // Validate the serialized result, not only the authoring group's checkboxes.
        internal static void Validate(ContentCatalogData data)
        {
            var settings=AddressableAssetSettingsDefaultObject.Settings;
            if(settings==null)throw new BuildFailedException("Addressables settings missing during catalog validation.");
            var locator=data.CreateLocator();
            var missing=new List<string>();int checkedCount=0;
            foreach(var group in settings.groups.Where(g=>g!=null&&g.name.StartsWith("Metin2-",StringComparison.Ordinal)&&!g.name.Contains("Shared")&&g.name!="Metin2-Core"))
                foreach(var entry in group.entries)
                {
                    if(string.IsNullOrEmpty(entry.address)||entry.IsFolder)continue;
                    checkedCount++;
                    if(!locator.Locate(entry.address,null,out var locations)||locations==null||locations.Count==0)missing.Add(group.name+": "+entry.address);
                }
            if(missing.Count>0)throw new BuildFailedException("Runtime addresses missing from built catalog ("+missing.Count+" / "+checkedCount+"): "+string.Join("; ",missing.Take(12)));
            UnityEngine.Debug.Log("METIN2_CATALOG_ADDRESSES_VALID count="+checkedCount);
        }
    }
}
