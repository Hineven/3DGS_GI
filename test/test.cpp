/*
 * Created: 2024/11/7
 * Author:  hineven
 * See LICENSE for licensing.
 */
#include "happly.h"
#include "gfx_window.h"
int main () {
// Construct a data object by reading from file
    happly::PLYData plyIn("../data/garden/input.ply");

// Get data from the object
    std::vector<float> elementA_prop1 = plyIn.getElement("vertex").getProperty<float>("x");

    gfxCommandSortRadix()


    int b = 1;
    return 0;
}