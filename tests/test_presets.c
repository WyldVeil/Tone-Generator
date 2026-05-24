#include "presets.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

static const Preset* find(const char* name) {
    int count = 0;
    const Preset* list = presets_list(&count);
    for (int i = 0; i < count; i++) {
        if (strcmp(list[i].name, name) == 0) return &list[i];
    }
    return 0;
}

int main(void) {
    int count = 0;
    const Preset* list = presets_list(&count);
    assert(list != 0);
    assert(count >= 3);   /* at least the seed entries */

    const Preset* schumann = find("Schumann resonance");
    assert(schumann != 0);
    assert(schumann->hz == 7.83);

    const Preset* a440 = find("Concert A (standard)");
    assert(a440 != 0);
    assert(a440->hz == 440.0);

    const Preset* solf528 = find("Solfeggio MI (528)");
    assert(solf528 != 0);
    assert(solf528->hz == 528.0);

    printf("test_presets: %d presets, 3 named lookups passed\n", count);
    return 0;
}
