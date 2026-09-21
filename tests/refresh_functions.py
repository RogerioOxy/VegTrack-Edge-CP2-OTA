from pathlib import Path
root = Path(__file__).resolve().parent.parent
source = (root / 'firmware_v2.ino').read_text(encoding='utf-8')
target = root / 'tests/actual_functions_test.cpp'
test = target.read_text(encoding='utf-8')
for start, end in [('// BEGIN PURE FUNCTIONS', '// END PURE FUNCTIONS'),
                   ('// BEGIN V2 PURE FUNCTIONS', '// END V2 PURE FUNCTIONS')]:
    # Substitui o trecho inteiro, incluindo a marca final; nenhuma reimplementação.
    block = source[source.index(start):source.index(end) + len(end)]
    begin = test.index(start)
    if end in test[begin:]:
        finish = test.index(end, begin) + len(end)
    else:
        finish = test.index('// BEGIN V2 PURE FUNCTIONS', begin) if 'V2' not in start else test.index('int failures=', begin)
    test = test[:begin] + block + '\n' + test[finish:]
target.write_text(test, encoding='utf-8')
print('Functions refreshed directly from firmware_v2.ino')
