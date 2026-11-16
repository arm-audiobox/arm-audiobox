import importlib.util
from pathlib import Path
import re
import unittest
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('po_tools',ROOT/'tools/build_translations.py')
po=importlib.util.module_from_spec(spec)
# Run the library part without PlatformIO's build hook.
source=(ROOT/'tools/build_translations.py').read_text().split("if __name__ == '__main__':")[0]
exec(compile(source,str(ROOT/'tools/build_translations.py'),'exec'),po.__dict__)
class CatalogTests(unittest.TestCase):
 def test_catalogs_have_matching_keys(self):
  self.assertEqual(set(po.parse_po(ROOT/'locale/de.po')),set(po.parse_po(ROOT/'locale/en.po')))
 def test_placeholders_survive_translation(self):
  for file in (ROOT/'locale').glob('*.po'):
   for key,value in po.parse_po(file).items():
    if key:self.assertEqual(set(re.findall(r'\{(\w+)\}',key)),set(re.findall(r'\{(\w+)\}',value)),(file,key))
 def test_all_setup_template_labels_exist(self):
  keys=po.parse_po(ROOT/'locale/en.po');source=(ROOT/'src/main.cpp').read_text()
  template=source.split('R"HTML(',1)[1].split(')HTML"',1)[0]
  for key in re.findall(r'\{\{(.*?)\}\}',template):self.assertIn(key,keys)
 def test_german_umlauts_and_multiline_header(self):
  catalog=po.parse_po(ROOT/'locale/de.po');self.assertEqual(catalog['Back'],'Zurück');self.assertIn('Language: de\n',catalog[''])
if __name__=='__main__':unittest.main()
