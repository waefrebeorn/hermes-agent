#ifndef HERMES_JSON_H
#define HERMES_JSON_H

/*
 * slermes_json.h — Compatibility shim: maps old slermes JSON API → libjson.
 * 
 * Old API used json_node_t / json_new_* / json_object_* etc.
 * New libjson uses json_t / json_* / json_obj_* etc.
 * This header provides backward-compatible typedefs + macros.
 */

#include "../lib/libjson/json.h"

/* Type alias: old json_node_t → new json_t */
typedef json_t json_node_t;

/* Builder macros: old json_new_* → new short names */
#define json_new_null()         json_null()
#define json_new_bool(v)        json_bool(v)
#define json_new_number(v)      json_number(v)
#define json_new_string(v)      json_string(v)
#define json_new_array()        json_array()
#define json_new_object()       json_object()

/* Array ops: old json_array_* → new json_* */
#define json_array_append(a,i)  json_append((a),(i))
#define json_array_get(a,i)     json_get((a),(i))
#define json_array_count(a)     json_len((a))

/* Object ops: old json_object_* → new json_obj_* */
#define json_object_set(o,k,v)  json_set((o),(k),(v))
#define json_object_get(o,k)    json_obj_get((o),(k))
#define json_object_get_string(o,k,d) json_get_str((o),(k),(d))
#define json_object_get_number(o,k,d) json_get_num((o),(k),(d))
#define json_object_get_bool(o,k,d)   json_get_bool((o),(k),(d))

#endif /* HERMES_JSON_H */
