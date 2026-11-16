"""Compile UTF-8 gettext PO menu catalogs into a small firmware lookup table."""
from pathlib import Path
import ast
import json

def parse_po(path):
    messages = {}
    msgid = msgstr = None
    current = None
    fuzzy = False
    def commit():
        if msgid is not None and msgstr is not None and not fuzzy:
            if msgid in messages:
                raise ValueError(f'{path}: duplicate msgid {msgid!r}')
            messages[msgid] = msgstr
    for number, raw in enumerate(Path(path).read_text(encoding='utf-8').splitlines(), 1):
        line = raw.strip()
        if not line:
            commit(); msgid = msgstr = current = None; fuzzy = False
        elif line.startswith('#,'):
            fuzzy = fuzzy or 'fuzzy' in [part.strip() for part in line.split(',')]
        elif line.startswith('#'):
            continue
        elif line.startswith('msgid '):
            if msgid is not None:
                commit(); msgstr = None; fuzzy = False
            msgid = ast.literal_eval(line[6:]); current = 'id'
        elif line.startswith('msgstr '):
            msgstr = ast.literal_eval(line[7:]); current = 'str'
        elif line.startswith('"'):
            if current == 'id': msgid += ast.literal_eval(line)
            elif current == 'str': msgstr += ast.literal_eval(line)
            else: raise ValueError(f'{path}:{number}: orphan continuation')
        else:
            raise ValueError(f'{path}:{number}: unsupported PO syntax')
    commit()
    return messages

def generate(root, output):
    root = Path(root)
    catalogs = {p.stem: parse_po(p) for p in sorted((root/'locale').glob('*.po'))}
    if 'en' not in catalogs or 'de' not in catalogs: raise ValueError('English and German catalogs are required')
    keys = sorted(set().union(*(set(v)-{''} for v in catalogs.values())))
    codes = list(catalogs)
    names = []
    for code, catalog in catalogs.items():
        metadata = dict(line.split(':', 1) for line in catalog.get('', '').splitlines() if ':' in line)
        names.append(metadata.get('Language-Name', code).strip())
    quote = lambda s: json.dumps(s, ensure_ascii=False)
    lines = ['#pragma once', '#include <cstring>', '#include <cstddef>', 'namespace I18n {',
             f'constexpr size_t languageCount={len(codes)};',
             'static const char* const languageCodes[]={'+','.join(map(quote,codes))+'};',
             'static const char* const languageNames[]={'+','.join(map(quote,names))+'};',
             'struct Message {const char* id; const char* text[languageCount];};',
             'static const Message messages[]={']
    for key in keys:
        values = [catalogs[c].get(key) or catalogs['en'].get(key) or key for c in codes]
        lines.append('{'+quote(key)+',{'+','.join(map(quote,values))+'}},')
    lines += ['};', '''inline int languageIndex(const char* code){for(size_t n=0;n<languageCount;n++)if(std::strcmp(code,languageCodes[n])==0)return int(n);return -1;}
inline const char* lookup(const char* code,const char* source){int lang=languageIndex(code);if(lang<0)lang=languageIndex("en");for(const auto& message:messages){bool match=std::strcmp(source,message.id)==0;for(size_t n=0;!match&&n<languageCount;n++)match=std::strcmp(source,message.text[n])==0;if(match)return message.text[lang];}return nullptr;}
}''']
    output = Path(output); output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text('\n'.join(lines)+'\n',encoding='utf-8')
    print(f'Translations: {len(keys)} messages, {len(codes)} PO catalogs')

if __name__ == '__main__':
    import argparse
    parser=argparse.ArgumentParser();parser.add_argument('--root',default='.');parser.add_argument('--out',required=True)
    args=parser.parse_args();generate(args.root,args.out)
else:
    Import('env')
    target=Path(env.subst('$BUILD_DIR'))/'generated'
    generate(env.subst('$PROJECT_DIR'),target/'i18n_generated.h')
    env.Append(CPPPATH=[str(target)])
