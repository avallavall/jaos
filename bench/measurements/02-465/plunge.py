import sys
p = sys.argv[1] + '/src/mip.c'
t = open(p, encoding='utf-8').read()

def sub(old, new):
    global t
    assert t.count(old) == 1, (old[:70], t.count(old))
    t = t.replace(old, new)

i = t.index('        bnode *up = node_child(nodes > 0 ? cur : nullptr, nc, nr_child,')
j = t.index(';', i)
stmt = t[i:j + 1]
t = t[:j + 1] + '''
        if (down != nullptr && up != nullptr) {
            pl_down = down->id;
            pl_up = up->id;
        }''' + t[j + 1:]
sub('''        nodes++;
''', '''        nodes++;
        if (cur != nullptr) {
            pl_n++;
            if (cur->id == pl_down || cur->id == pl_up)
                pl_child++;
        }
''')
sub('''static jaos_status bb_tree(jaos_model *m, bb_restart *rs)
{''', '''static long long pl_n, pl_child;
static int64_t pl_down = -1, pl_up = -1;
static bool pl_armed;
static void pl_print(void)
{
    fprintf(stderr, "PLUNGE nodes=%lld child_of_previous=%lld\\n", pl_n, pl_child);
}

static jaos_status bb_tree(jaos_model *m, bb_restart *rs)
{
    if (!pl_armed) {
        pl_armed = true;
        atexit(pl_print);
    }''')
for inc in ('#include <stdio.h>', '#include <stdlib.h>'):
    if inc not in t:
        t = inc + '\n' + t
open(p, 'w', encoding='utf-8').write(t)
print('plunge ok')
