"""Finds locals that hide an enclosing local -- MSVC C4456, a build error here.

A previous version flagged every second `for (int32 I ...)` in a file, because it
put the loop variable in the surrounding scope. A loop variable belongs to a scope
that covers the loop and ends with it, so two sibling loops never collide while a
nested one does. Modelling that is the difference between a check worth reading
and a check you learn to ignore.
"""
import re, sys

DECL = re.compile(
    r'\b(?:const\s+)?(?:auto|bool|int32|int64|uint32|float|double|FString|FName|FText'
    r'|FVector2D|FVector|FRotator|FTransform|FLinearColor|FColor|FQuat'
    r'|EAH\w+|FAH\w+|T\w+\s*<[^;{}]*?>|[AUFE][A-Z]\w+)\s*[*&]?\s+(\w+)\s*(?==[^=]|\s*[;({:])')
CONTROL = re.compile(r'\b(for|if|while|switch|catch)\s*\(')
KEYWORDS = {'if','for','while','return','else','case','switch','new','delete','const','class','struct'}

def scan(path, report):
    src = open(path, encoding='utf-8', errors='replace').read()
    src = re.sub(r'//[^\n]*', '', src)
    src = re.sub(r'/\*.*?\*/', lambda m: '\n' * m.group(0).count('\n'), src, flags=re.S)
    src = re.sub(r'"(\\.|[^"\\])*"', '""', src)

    frames = [{'vars': {}, 'owns': 0}]
    pending_control = 0      # control scopes opened, waiting for their body
    hits, line, i = 0, 1, 0
    while i < len(src):
        ch = src[i]
        if ch == '\n':
            line += 1; i += 1; continue
        m = CONTROL.match(src, i)
        if m:
            frames.append({'vars': {}, 'owns': 0})   # scope of the loop/if itself
            pending_control += 1
            i = m.end(); continue
        if ch == '{':
            frames.append({'vars': {}, 'owns': pending_control})
            pending_control = 0
            i += 1; continue
        if ch == '}':
            if len(frames) > 1:
                owed = frames[-1]['owns']; frames.pop()
                for _ in range(owed):
                    if len(frames) > 1: frames.pop()
            i += 1; continue
        if ch == ';' and pending_control:
            # a control statement with no braces: its scope ends here
            for _ in range(pending_control):
                if len(frames) > 1: frames.pop()
            pending_control = 0
            i += 1; continue
        d = DECL.match(src, i)
        if d:
            name = d.group(1)
            if name not in KEYWORDS:
                for scope in frames[:-1]:
                    if name in scope['vars']:
                        report.append(f"{path}:{line}  '{name}' esconde a declaracao da linha {scope['vars'][name]}")
                        hits += 1
                        break
                frames[-1]['vars'][name] = line
            line += src.count('\n', i, d.end()); i = d.end(); continue
        i += 1
    return hits

report = []
total = sum(scan(p, report) for p in sys.argv[1:])
for r in report: print('  ' + r)
print(f"total: {total}")
