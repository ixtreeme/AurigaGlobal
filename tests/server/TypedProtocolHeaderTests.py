"""Typed extensions must coexist with every declared legacy header, including hex IDs."""
from pathlib import Path
import re,unittest
ROOT=Path(__file__).resolve().parents[2]
class TypedProtocolHeaders(unittest.TestCase):
    def test_extension_ids_do_not_alias_legacy_packets(self):
        for direction,name,value in [('CG','TYPED_SPECIAL_BONUSES',214),('GC','TYPED_SPECIAL_BONUSES',220),('GC','TYPED_APPEARANCE',241)]:
            text=(ROOT/'SRC/Server/common/Headers'/('Header_'+direction+'.h')).read_text(encoding='utf-8')
            text=re.sub(r'/\*.*?\*/|//[^\r\n]*','',text,flags=re.S)
            entries=[(symbol,int(number,0)) for symbol,number in re.findall(r'(HEADER_'+direction+r'_\w+)\s*=\s*(0x[0-9A-Fa-f]+|\d+)',text)]
            self.assertIn(('HEADER_'+direction+'_'+name,value),entries)
            self.assertEqual([symbol for symbol,number in entries if number==value],['HEADER_'+direction+'_'+name])
if __name__=='__main__':unittest.main()
