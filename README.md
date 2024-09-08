# 🐇BunLDTK
A Cool c99 LDTK Level Loader made with json-c

## ✅ Features:
- Overly commented header file
- Wall Greedy Meshing
- Single and multi file support
- Supports all world layouts
- Get custom fields easily with ldtk_get_field_lvl() and ldtk_get_field_ent() functions
- Reads data into simple to use C structs

## 💾 Usage 
- To add to your project simply copy the headers, bunarr.c and ldtk.c 

## ⚠️  Caveats:
- Currently not feature complete!
- Requires bunarr and json-c
- Walls w and h are relative to the minimum wall size, 
this means you must multiply the w and h by the tile size of your game before using them.

## 🗒️ Todo:
- Add proper support for different tile sizes and stuff
- Add Examples
- Add Super Simple Export Support
- Add PNG Level Export Support
- Add PNG Layer Export Support
- Proper error handling
- Generate docs with doxygen???
