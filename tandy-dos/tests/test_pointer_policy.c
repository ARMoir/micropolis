/* SPDX-License-Identifier: GPL-3.0-or-later
 * No arrow over the solid yellow city-selection square.
 */
#include "../pointer_policy.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    /* Entire 32x21-tile map should use ONLY the yellow tile cursor. */
    assert(!pointer_should_show(64U,16U,0,0));
    assert(!pointer_should_show(319U,183U,0,0));
    assert(!pointer_should_show(160U,100U,0,0));
    /* Toolbar, menu bar, and footer still need a normal arrow. */
    assert(pointer_should_show(63U,100U,0,0));
    assert(pointer_should_show(160U,15U,0,0));
    assert(pointer_should_show(160U,184U,0,0));
    /* Dropdowns and help overlays lie over the city and need an arrow. */
    assert(pointer_should_show(160U,100U,1,0));
    assert(pointer_should_show(160U,100U,0,1));
    /* Closing the overlay returns to the city-only tile cursor. */
    assert(!pointer_should_show(160U,100U,0,0));
    puts("Tandy arrow/square cursor visibility policy passed");
    return 0;
}
