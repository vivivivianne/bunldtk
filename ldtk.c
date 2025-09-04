/** ldtk.c - API for loading Ldtk Levels, 
* reads .json ldtk levels and returns  
* it in the form of a filled ldtk_Level struct */

#include <string.h>
#include <libgen.h> // provides basename()
#include <stdio.h>
#include "ldtk.h"

static i32 json_get_i32(json_object *obj, char *key);
static void *json_get_ptr(json_object *obj, char *key);
static char *json_get_str(json_object *obj, char *key);

static json_object *get_lvl_json(char *name);
static void get_intgrid(ldtk_lvl *lvl, json_object *gridLayer);
static void get_tile(ldtk_lvl *lvl, bunlist *tiles, json_object *tile_i);
static void get_tilelayer(ldtk_lvl *lvl, json_object *Layer, char *tilekey);
static void get_ents(ldtk_lvl *lvl, json_object *entitiyLayer);
static void get_ngbrs(ldtk_lvl *lvl, json_object *parsed_json);

static void arr_to_grid(i32 *csvgrid, i32 lenx, i32 leny, i32 grid[lenx][leny]);
static void grid_to_walls(i32 lenx, i32 leny, i32 grid[lenx][leny],
			  ldtk_lvl *lvl);
static void grid_to_walls_greedy(i32 lenx, i32 leny, i32 grid[lenx][leny],
				 ldtk_lvl *lvl);
static i32 expand_y(ldtk_rect row, i32 lenx, i32 leny, i32 grid[lenx][leny]);
static bool expand_x(ldtk_rect row, i32 lenx, i32 leny,
		     i32 intgrid[lenx][leny]);
static void free_neighbours(usize i, void *itm);
static void free_layers(usize i, void *itm);
static void free_ents(usize i, void *itm);
static bool chk_flag(i32 flag, i32 bit);
static bool ldtk_grid_value_accepted(u32 value);

static ldtk_sys sys;
static u32 z = 0;

enum : u16 {
	LDTK_SINGLE_FILE = 0x00000001, /**< Single file contains all levels*/
	LDTK_MULTI_FILE = 0x00000002, /**< Each level has their file */
	LDTK_PNG_LAYER = 0x00000004, /**< one png for each layer of each level*/
	LDTK_PNG_LEVEL = 0x00000008, /**< Generates pngs for each level*/
	LDTK_WORLD_GRIDVANIA = 0x00000010, /**< Loads a gridvania world*/
	LDTK_WORLD_HORIZONTAL = 0x00000020, /**< Loads a horizontal world*/
	LDTK_WORLD_VERTICAL = 0x00000040, /**< Loads a vertical world */
	LDTK_WORLD_FREE_MAP = 0x0000080, /**< Loads a free world */
	LDTK_SIMPLE_EXPORT = 0x00001000, /**< Enables Super Simple Export*/
	LDTK_PNG_BOTH = LDTK_PNG_LEVEL | LDTK_PNG_LAYER, /**< Png for both */
};

void ldtk_init(u32 tl_size, char *prj_name, char *prj_dir, LDTK_FLAGS flags)
{
	ldtk_sys ldtk_sys;
	ldtk_sys.tl_size = tl_size;
	ldtk_sys.prj_dir = prj_dir;
	ldtk_sys.prj_name = prj_name;
	ldtk_sys.flags = flags;

	char jsonpath[300] = "";
	strcat(jsonpath, ldtk_sys.prj_dir);
	strcat(jsonpath, ldtk_sys.prj_name);

	if (chk_flag(flags, LDTK_EXTENSION_JSON))
		strcat(jsonpath, ".json");
	else if (chk_flag(flags, LDTK_EXTENSION_LDTK))
		strcat(jsonpath, ".ldtk");
	json_object *json = json_object_from_file(jsonpath);

	char *layout = json_get_str(json, "worldLayout");
	if (strcmp(layout, "gridvania") == 0) {
		ldtk_sys.flags |= LDTK_WORLD_GRIDVANIA;
	} else if (strcmp(layout, "horizontal") == 0) {
		ldtk_sys.flags |= LDTK_WORLD_HORIZONTAL;
	} else if (strcmp(layout, "vertical") == 0) {
		ldtk_sys.flags |= LDTK_WORLD_VERTICAL;
	} else if (strcmp(layout, "free") == 0) {
		ldtk_sys.flags |= LDTK_WORLD_FREE_MAP;
	}
	free(layout);
	bool *external_levels = (json_get_ptr(json, "externalLevels"));
	if (*external_levels) {
		ldtk_sys.flags |= LDTK_MULTI_FILE;
	} else {
		ldtk_sys.flags |= LDTK_SINGLE_FILE;
	}
	free(external_levels);

	bool *simple_export = (json_get_ptr(json, "simplifiedExport"));
	if (*simple_export) {
		ldtk_sys.flags |= LDTK_SINGLE_FILE;
	}
	free(simple_export);
	// if no extension is selected we use the ldtk extension by default
	if ((ldtk_sys.flags & LDTK_EXTENSION_JSON) != LDTK_EXTENSION_JSON) {
		ldtk_sys.flags |= LDTK_EXTENSION_LDTK;
	}
	char *image_export = json_get_str(json, "imageExportMode");

	if (strcmp(image_export, "LayersAndLevels") == 0) {
		ldtk_sys.flags |= LDTK_PNG_BOTH;
	} else if (strcmp(image_export, "Levels") == 0) {
		ldtk_sys.flags |= LDTK_PNG_LEVEL;
	} else if (strcmp(image_export, "Layers") == 0) {
		ldtk_sys.flags |= LDTK_PNG_LAYER;
	}
	free(image_export);
	json_object_put(json);

	sys = ldtk_sys;
	sys.ignored_intgrid_values = bunlist_create(sizeof(u32), 10, NULL);
	ldtk_ignore_intgrid_value(0);
}

void ldtk_free(void)
{
	bunlist_destroy(sys.ignored_intgrid_values);
}

void ldtk_get_lvl_name(char *iid, char *dst)
{
	char path[300] = "";
	strcat(path, sys.prj_dir);
	strcat(path, sys.prj_name);

	i32 flags = sys.flags;

	if (chk_flag(flags, LDTK_EXTENSION_JSON))
		strcat(path, ".json");
	else if (chk_flag(flags, LDTK_EXTENSION_LDTK))
		strcat(path, ".ldtk");

	json_object *json = json_object_from_file(path);
	json_object *lvls = json_object_object_get(json, "levels");
	i32 len = json_object_array_length(lvls);
	for (i32 i = 0; i < len; i++) {
		json_object *lvl_i = json_object_array_get_idx(lvls, i);
		u32 type = json_object_get_type(lvl_i);

		char *iid_i = json_get_str(lvl_i, "iid");

		if (iid == NULL || type == json_type_null)
			break;

		if (strcmp(iid, iid_i) == 0) {
			char *fpath = json_get_str(lvl_i, "identifier");
			strcpy(dst, fpath);
			free(fpath);
		}

		free(iid_i);
	}
	json_object_put(json);
}

ldtk_lvl *ldtk_load_lvl(char *lname)
{
	json_object *lvl_json = get_lvl_json(lname);

	if (lvl_json == NULL) {
		return NULL;
	}
	ldtk_lvl *lvl = malloc(sizeof(ldtk_lvl));
	memset(lvl, 0, sizeof(ldtk_lvl));

	lvl->id = json_get_str(lvl_json, "iid");
	lvl->bg_tile_path = json_get_str(lvl_json, "bgRelPath");

	lvl->rect.x = json_get_i32(lvl_json, "worldX");
	lvl->rect.y = json_get_i32(lvl_json, "worldY");
	lvl->rect.w = json_get_i32(lvl_json, "pxWid");
	lvl->rect.h = json_get_i32(lvl_json, "pxHei");

	lvl->layers = bunlist_create(sizeof(ldtk_layer), 5, free_layers);
	lvl->ngbrs = bunlist_create(sizeof(ldtk_ngbr), 6, free_neighbours);
	lvl->walls = bunlist_create(sizeof(ldtk_wall), 60, NULL);

	lvl->path = malloc(sizeof(char) * 300);
	ldtk_get_lvl_name(lvl->id, lvl->path);
	get_ngbrs(lvl, lvl_json);

	lvl->custom_fields = json_object_object_get(lvl_json, "fieldInstances");
	json_object_get(lvl->custom_fields);

	// try to get bg_color (from the lvl), if it is null use __bgcolor (from the world)
	char *hex_color = json_get_str(lvl_json, "bgColor");
	if (hex_color == NULL) {
		hex_color = json_get_str(lvl_json, "__bgColor");
	}
	sscanf(&hex_color[1], "%02hhx%02hhx%02hhx", &lvl->r, &lvl->g, &lvl->b);
	if (hex_color != NULL) {
		free(hex_color);
	}

	json_object *layers =
		json_object_object_get(lvl_json, "layerInstances");
	i32 layers_len = json_object_array_length(layers);

	// load each layer
	for (i32 i = 0; i < layers_len; i++) {
		z = i;
		json_object *arr_i = json_object_array_get_idx(layers, i);
		char *layer_str = json_get_str(arr_i, "__type");
		if (layer_str == NULL)
			break;
		if (strcmp(layer_str, "AutoLayer") == 0) {
			get_tilelayer(lvl, arr_i, "autoLayerTiles");
		} else if (strcmp(layer_str, "Tiles") == 0) {
			get_tilelayer(lvl, arr_i, "gridTiles");
		} else if (strcmp(layer_str, "IntGrid") == 0) {
			get_tilelayer(lvl, arr_i, "autoLayerTiles");
			get_intgrid(lvl, arr_i);
		} else if (strcmp(layer_str, "Entities") == 0) {
			get_ents(lvl, arr_i);
		}
		free(layer_str);
	}

	json_object_put(lvl_json);

	return lvl;
}

void ldtk_destroy_lvl(ldtk_lvl *lvl)
{
	ldtk_destroy_lvl_ex(lvl, 0);
}

void ldtk_destroy_lvl_ex(ldtk_lvl *lvl, LDTK_LVL_FLAGS flags)
{
	flags |= 0;

	if (!chk_flag(flags, LVL_KEEP_FIELDS)) {
		json_object_put(lvl->custom_fields);
	}
	for (u32 i = 0; i < lvl->layers->len; i++) {
		ldtk_layer *layer_i = bunlist_get(lvl->layers, i);
		if (!chk_flag(flags, LVL_KEEP_TILES) &&
		    layer_i->type == LDTK_LAYER_TILES) {
			bunlist_destroy(layer_i->content);
		}
		if (layer_i->tileset_path != NULL) {
			free(layer_i->tileset_path);
		}
		if (layer_i->composite != NULL) {
			free(layer_i->composite);
		}
		if (layer_i->type == LDTK_LAYER_ENTITY) {
			bunlist_destroy(layer_i->content);
		}
	}
	if (!chk_flag(flags, LVL_KEEP_TILES)) {
		bunlist_destroy(lvl->layers);
	}

	if (!chk_flag(flags, LVL_KEEP_PATH)) {
		free(lvl->path);
	}

	if (!chk_flag(flags, LVL_KEEP_NGBR)) {
		for (u32 i = 0; i < lvl->ngbrs->len; i++) {
			ldtk_ngbr *ngbr = bunlist_get(lvl->ngbrs, i);
			free(ngbr->path);
		}
		bunlist_destroy(lvl->ngbrs);
	}
	bunlist_destroy(lvl->walls);
	free(lvl->bg_tile_path);
	free(lvl->id);
	free(lvl);
}

void *ldtk_get_field(json_object *custom_fields, char *field)
{
	void *var = NULL;
	u32 len = json_object_array_length(custom_fields);
	for (u32 i = 0; i < len; i++) {
		json_object *field_i =
			json_object_array_get_idx(custom_fields, i);
		char *ident = json_get_str(field_i, "__identifier");
		if (ident == NULL)
			break;

		if (strcmp(ident, field) == 0) {
			var = json_get_ptr(field_i, "__value");
		}
		free(ident);
	}
	return var;
}

void *ldtk_get_lvl_field(ldtk_lvl *lvl, char *field)
{
	return ldtk_get_field(lvl->custom_fields, field);
}

void *ldtk_get_ent_field(ldtk_ent *ent, char *field)
{
	return ldtk_get_field(ent->custom_fields, field);
}

/** \brief get the json object of a level with the given name */
static json_object *get_lvl_json(char *name)
{
	json_object *parsed_json = NULL;
	char path_final[300] = "";
	if (chk_flag(sys.flags, LDTK_MULTI_FILE)) {
		char lvl_dir[300] = "";
		strcat(lvl_dir, sys.prj_dir);
		strcat(lvl_dir, sys.prj_name);
		strcat(lvl_dir, "/");
		strcat(path_final, lvl_dir);
		strcat(path_final, name);
		strcat(path_final, ".ldtkl");

		parsed_json = json_object_from_file(path_final);
		return parsed_json;

	} else if (chk_flag(sys.flags, LDTK_SINGLE_FILE)) {
		strcat(path_final, sys.prj_dir);
		strcat(path_final, sys.prj_name);

		if (chk_flag(sys.flags, LDTK_EXTENSION_JSON))
			strcat(path_final, ".json");
		else if (chk_flag(sys.flags, LDTK_EXTENSION_LDTK))
			strcat(path_final, ".ldtk");
		json_object *main_file = json_object_from_file(path_final);

		json_object *lvls = json_object_object_get(main_file, "levels");
		i32 len = json_object_array_length(lvls);
		for (i32 i = 0; i < len; i++) {
			json_object *lvl_i = json_object_array_get_idx(lvls, i);
			u32 type = json_object_get_type(lvl_i);
			if (type != json_type_null) {
				char *lvl = json_get_str(lvl_i, "identifier");
				if (strcmp(name, lvl) == 0) {
					free(lvl);
					json_object_get(lvl_i);
					return lvl_i;
				}
			}
		}
	}
	return parsed_json;
}

/** Loads the tiles of a tile layer*/
static void get_tilelayer(ldtk_lvl *lvl, json_object *Layer, char *tilekey)
{
	if (Layer != NULL) {
		char tsfolder[300] = "assets/Tiles/";
		char *p = json_get_str(Layer, "__tilesetRelPath");
		char *bname = basename(p);
		strcat(tsfolder, bname);
		char *layer_identifier = json_get_str(Layer, "__identifier");
		char identifier[300];
		strcat(identifier, layer_identifier);
		strcat(identifier, "\0");

		bunlist *content = bunlist_create(sizeof(ldtk_tile), 400, NULL);
		i32 tilesize = json_get_i32(Layer, "__gridSize");
		ldtk_layer tl = { .type = LDTK_LAYER_TILES,
				  .z = z,
				  .tilesize = tilesize,
				  .tileset_path = strdup(tsfolder),
				  .composite = NULL,
				  .content = content,
				  .identifier = strdup(identifier) };
		json_object *tiles = json_object_object_get(Layer, tilekey);
		i32 len = json_object_array_length(tiles);
		for (i32 j = 0; j < len; j++) {
			json_object *tiles_j;
			tiles_j = json_object_array_get_idx(tiles, j);
			get_tile(lvl, tl.content, tiles_j);
		}

		bunlist_append(lvl->layers, &tl);
		free(p);
		free(layer_identifier);
	}
}

/** Loads The intgrids */
static void get_intgrid(ldtk_lvl *lvl, json_object *gridLayer)
{
	json_object *grid = NULL;

	grid = json_object_object_get(gridLayer, "intGridCsv");

	i32 len = json_object_array_length(grid);
	i32 lenx = json_get_i32(gridLayer, "__cWid");
	i32 leny = json_get_i32(gridLayer, "__cHei");

	i32 csvgrid[len];

	for (i32 i = 0; i < len; i++) {
		json_object *grid_i = json_object_array_get_idx(grid, i);
		i32 value = json_object_get_int(grid_i);
		csvgrid[i] = value;
	}

	// first we need to convert from a one line array (csvgrid) to an actual grid (intgrid)
	i32 intgrid[lenx][leny] = {};
	memset(intgrid, 0, sizeof(intgrid));

	arr_to_grid(csvgrid, lenx, leny, intgrid);
	if (chk_flag(sys.flags, LDTK_LEVEL_GREEDY_MESH)) {
		grid_to_walls_greedy(lenx, leny, intgrid, lvl);
	} else {
		grid_to_walls(lenx, leny, intgrid, lvl);
	}
}

// creates and appends tiles to the given tile list
static void get_tile(ldtk_lvl *lvl, bunlist *tiles, json_object *tile_i)
{
	json_object *jpx = json_object_object_get(tile_i, "px");
	json_object *jsrc = json_object_object_get(tile_i, "src");
	json_object *jt = json_object_object_get(tile_i, "t");
	json_object *jf = json_object_object_get(tile_i, "f");
	i32 t, f, x, y, sx, sy;
	x = lvl->rect.x +
	    json_object_get_int(json_object_array_get_idx(jpx, 0));
	y = lvl->rect.y +
	    json_object_get_int(json_object_array_get_idx(jpx, 1));
	sx = json_object_get_int(json_object_array_get_idx(jsrc, 0));
	sy = json_object_get_int(json_object_array_get_idx(jsrc, 1));
	t = json_object_get_int(jt);
	f = json_object_get_int(jf);

	ldtk_tile tile = { { x, y, 0, 0 }, { sx, sy, 0, 0 }, t, f };

	bunlist_append(tiles, &tile);
	json_object_object_del(tile_i, "px");
	json_object_object_del(tile_i, "src");
	json_object_object_del(tile_i, "t");
	json_object_object_del(tile_i, "f");
}
/** \brief Creates the entity array inside the ldtk level */
static void get_ents(ldtk_lvl *lvl, json_object *entityLayer)
{
	json_object *entities;

	char *layer_identifier = json_get_str(entityLayer, "__identifier");
	char identifier[300];
	strcat(identifier, layer_identifier);
	// strcat(identifier, "\0");
	entities = json_object_object_get(entityLayer, "entityInstances");
	i32 len = json_object_array_length(entities);
	ldtk_layer layer;
	layer.type = LDTK_LAYER_ENTITY;
	layer.z = z;
	layer.tileset_path = NULL;
	layer.composite = NULL;
	layer.tilesize = 0;
	layer.identifier = strdup(layer_identifier);
	layer.content = bunlist_create(sizeof(ldtk_ent), 10, free_ents);
	for (i32 i = 0; i < len; i++) {
		json_object *ent;

		ent = json_object_array_get_idx(entities, i);

		i32 r, g, b; // get color
		char *hex_color = json_get_str(ent, "__smartColor");
		sscanf(&hex_color[1], "%02x%02x%02x", &r, &g, &b);
		free(hex_color);
		ldtk_rect rt; // get rect
		rt.w = json_get_i32(ent, "width");
		rt.h = json_get_i32(ent, "height");
		rt.x = json_get_i32(ent, "__worldX");
		rt.y = json_get_i32(ent, "__worldY");

		json_object *field_instances = json_object_get(
			json_object_object_get(ent, "fieldInstances"));

		ldtk_ent f_ent = { .rect = rt,
				   .r = r,
				   .g = g,
				   .b = b,
				   .custom_fields = field_instances };

		bunlist_append(layer.content, &f_ent);
	}
	bunlist_append(lvl->layers, &layer);
	free(layer_identifier);
}

/** \brief convert a single line csvgrid to a 2D intgrid*/
static void arr_to_grid(i32 *csvgrid, i32 lenx, i32 leny,
			i32 intgrid[lenx][leny])
{
	i32 lastx = 0;
	for (i32 y = 0; y < leny; y++) {
		for (i32 x = 0; x < lenx; x++) {
			intgrid[x][y] = csvgrid[x + lastx];
		}

		lastx += lenx;
	}
}

/** \brief Parses the intgrid end calls your custom create_wall function for every tile */
static void grid_to_walls(i32 lenx, i32 leny, i32 intgrid[lenx][leny],
			  ldtk_lvl *lvl)
{
	for (i32 y = 0; y < leny; y++) {
		for (i32 x = 0; x < lenx; x++) {
			i32 val = intgrid[x][y];
			ldtk_rect rect = { x, y, 1, 1 };
			ldtk_wall wall = { rect, val };
			bunlist_append(lvl->walls, &wall);
		}
	}
}

/** set's an area of the grid to an value */
static void set_grid_area(ldtk_rect area, i32 lenx, i32 leny,
			  i32 intgrid[lenx][leny], i32 value)
{
	for (i32 y = area.y; y < area.y + area.h; y++) {
		for (i32 x = area.x; x < area.x + area.w; x++) {
			if (x <= lenx && y <= leny) {
				intgrid[x][y] = value;
			}
		}
	}
};

/** \brief returns wether it's possible to create row by expanding in the x direction
 * \param row the row we want to try to expand into 
 * \param lenx the x lenght of the grid
 * \param leny the y lenght of the gird
 * \param grid the grid
 * \returns bool wheter we can expand or not*/
static bool expand_x(ldtk_rect row, i32 lenx, i32 leny, i32 grid[lenx][leny])
{
	bool expand = false;
	i32 currx = 0;
	i32 nextx = 0;
	i32 cw = 1;
	i32 finalx = row.x + row.w;

	for (i32 x = row.x; x < finalx; x++) {
		currx = grid[x][row.y];
		if (x + 1 < lenx) {
			nextx = grid[x + 1][row.y];
		}
		if (currx == nextx && nextx != 0 && x + 1 < lenx) {
			cw++;
		}
		if ((currx != nextx) || (x + 1 == lenx)) {
			expand = (cw == row.w);
		}
	}
	return expand;
}

/** \brief Expands the row of a grid in the y direction until it 
 * reaches a different value or the grid limits
 * \param 
 * \param lenx x lenght of the grid
 * \param leny y lenght of the grid
 * \param grid[lenx][leny] the grid
 * \returns h the height we can expand on the y direction
 * **/
static i32 expand_y(ldtk_rect row, i32 lenx, i32 leny, i32 grid[lenx][leny])
{
	bool can_expand = true;

	i32 h = 1;
	i32 curry = 0;
	i32 nexty = 0;
	i32 y = row.y;
	i32 x = row.x;
	i32 w = row.w;
	while (can_expand) {
		curry = grid[row.x][y];
		if (y + 1 < leny) {
			nexty = grid[x][y + 1];
		} else {
			can_expand = false;
		}

		if (curry == nexty && nexty != 0 && y + 1 < leny) {
			ldtk_rect row = { x, y + 1, w, 1 };
			if (expand_x(row, lenx, leny, grid)) {
				h++;
			} else {
				can_expand = false;
			}
		}
		if ((curry != nexty) || (y + 1 >= leny)) {
			can_expand = false;
		}
		y++;
	}
	return h;
}

/** parses the intgrid, but with greedy meshing */
static void grid_to_walls_greedy(i32 lenx, i32 leny, i32 grid[lenx][leny],
				 ldtk_lvl *lvl)
{
	i32 w = 1;
	i32 currx = 0, nextx = 0;
	for (i32 y = 0; y < leny; y++) {
		for (i32 x = 0; x < lenx; x++) {
			currx = grid[x][y];
			if (x + 1 < lenx) {
				nextx = grid[x + 1][y];
			}
			if (currx == nextx && nextx != 0 && x + 1 < lenx) {
				w++;
			}
			if ((currx != nextx || x + 1 >= lenx)) {
				if (ldtk_grid_value_accepted(currx)) {
					ldtk_rect rect = { x + 1 - w, y, w, 1 };

					rect.h = expand_y(rect, lenx, leny,
							  grid);

					set_grid_area(rect, lenx, leny, grid,
						      0);

					ldtk_wall wall = { rect, currx };
					bunlist_append(lvl->walls, &wall);

					w = 1;
				}
			}
		}
	}
}

/** \brief This function gets the neighbours of 
 * the given level room and appends it to the 
 * level neighbours list*/
static void get_ngbrs(ldtk_lvl *lvl, json_object *parsed_json)
{
	json_object *ngbr = json_object_object_get(parsed_json, "__neighbours");
	i32 len = json_object_array_length(ngbr);
	for (i32 i = 0; i < len; i++) {
		json_object *ngbr_i = json_object_array_get_idx(ngbr, i);
		char *n_id = json_get_str(ngbr_i, "levelIid");
		char n_path[300];
		char *n_dir = json_get_str(ngbr_i, "dir");

		ldtk_get_lvl_name(n_id, n_path);

		ldtk_ngbr ngbr;
		ngbr.path = malloc(sizeof(char) * 300);
		strcpy(ngbr.path, n_path);
		strcpy(ngbr.dir, n_dir);

		bunlist_append(lvl->ngbrs, &ngbr);
		free(n_dir);
		free(n_id);
	}
}

static void free_ents(usize i, void *itm)
{
	ldtk_ent *ent_i = itm;
	json_object_put(ent_i->custom_fields);
}

static void free_layers(usize i, void *itm)
{
	ldtk_layer *lyr = itm;
	if (lyr->identifier != NULL) {
		free(lyr->identifier);
	}
}

static void free_neighbours(usize i, void *itm)
{
	ldtk_ngbr *ngbr = itm;
	free(ngbr->path);
}

static bool chk_flag(i32 flag, i32 bit)
{
	return ((flag & bit) == bit);
}

/* \brief small function to easily get the int value of the child node*/
static i32 json_get_i32(json_object *obj, char *key)
{
	return json_object_get_int(json_object_object_get(obj, key));
}

/* \brief small function to easily read a json object and return a valid string from it */
static char *json_get_str(json_object *parent, char *key)
{
	char *str = (char *)json_object_get_string(
		json_object_object_get(parent, key));
	if (str == NULL)
		return NULL;
	char *fstr = strdup(str);
	return fstr;
}

/* \brief gets the value of the requested key in the json_object and returns a pointer to it */
static void *json_get_ptr(json_object *obj, char *key)
{
	void *var = NULL;
	json_object *field = json_object_object_get(obj, key);
	enum json_type type = json_object_get_type(field);

	switch (type) {
	case json_type_int: {
		i32 value;
		value = json_object_get_int(field);
		var = malloc(sizeof(value));
		memcpy(var, &value, sizeof(value));
		break;
	}
	case json_type_boolean: {
		bool value;
		value = json_object_get_boolean(field);
		var = malloc(sizeof(value));
		memcpy(var, &value, sizeof(value));
		break;
	}
	case json_type_null: {
		var = NULL;
		break;
	}
	case json_type_double: {
		f64 value;
		value = json_object_get_double(field);
		var = malloc(sizeof(value));
		memcpy(var, &value, sizeof(value));
		break;
	}
	case json_type_string: {
		const char *str = json_object_get_string(field);
		if (str == NULL) {
			var = NULL;
		} else {
			var = strdup(str);
		}
		break;
	}
	case json_type_array: {
		// array_list *list = json_object_get_array(field);
		// convert this to bunlist
		// var = json_get_str(field_i, "__value");
		var = NULL;
		break;
	}
	case json_type_object: {
		// hash table stuff, not gonna implement now
		// json_object_get_object(field);
		var = NULL;
		break;
	}
	}

	return var;
}

void ldtk_ignore_intgrid_value(u32 value)
{
	bunlist_append(sys.ignored_intgrid_values, &value);
}

static bool ldtk_grid_value_accepted(u32 value)
{
	for (u32 i = 0; i < sys.ignored_intgrid_values->len; i++) {
		u32 *arrval = bunlist_get(sys.ignored_intgrid_values, i);
		if (*arrval == value) {
			return false;
		}
	}
	return true;
}
