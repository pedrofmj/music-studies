"""Resolve the shared Arturia profile catalog to SF2-backed layers."""

PATCH_SCHEMA = "music-studies/arturia-genre-patches/v1"
ROUTING_SCHEMA = "music-studies/arturia-shared-sf2-routing/v1"
ORGAN_PROFILE = "windows-organ-sf2"
ORGAN_PATCH = "organ-gospel"


def resolve_sf2_patch(patch_catalog, routing, patch_id):
    patches = patch_catalog.get("patches")
    substitutions = routing.get("sf2_substitutions")
    if not isinstance(patches, dict) or not isinstance(substitutions, dict):
        raise ValueError("catalog or routing tables are missing")
    seen = set()
    while True:
        if patch_id in seen or patch_id not in patches:
            raise ValueError("invalid or cyclic SF2 substitution: " + patch_id)
        seen.add(patch_id)
        patch = patches[patch_id]
        if not isinstance(patch, dict):
            raise ValueError("invalid patch definition: " + patch_id)
        if patch.get("engine") == "sf2":
            path = patch.get("path")
            label = patch.get("label")
            if not isinstance(path, str) or not path:
                raise ValueError("SF2 patch has no relative path: " + patch_id)
            if not isinstance(label, str) or not label:
                raise ValueError("SF2 patch has no display label: " + patch_id)
            return patch_id, patch
        patch_id = substitutions.get(patch_id)
        if not isinstance(patch_id, str):
            raise ValueError("non-SF2 patch has no SF2 substitution")


def build_profiles(patch_catalog, routing):
    if patch_catalog.get("schema") != PATCH_SCHEMA:
        raise ValueError("unsupported Arturia patch catalog schema")
    if routing.get("schema") != ROUTING_SCHEMA or routing.get(
            "source_catalog") != "arturia-genre-patches.json":
        raise ValueError("unsupported shared-SF2 routing schema")

    patches = patch_catalog.get("patches")
    genres = patch_catalog.get("genres")
    active_layers = routing.get("active_layers")
    substitutions = routing.get("sf2_substitutions")
    if not isinstance(patches, dict) or not isinstance(genres, dict) or \
            not isinstance(active_layers, dict) or \
            not isinstance(substitutions, dict) or not genres:
        raise ValueError("catalog or routing tables are missing")

    def resolved_patch_path(patch_id):
        return resolve_sf2_patch(patch_catalog, routing, patch_id)[1]["path"]

    def checked_layers(profile_id, layers):
        if not isinstance(layers, list) or not layers or len(layers) > 9:
            raise ValueError("invalid active layer list: " + profile_id)
        if any(type(layer) is not int or layer < 1 or layer > 9
               for layer in layers) or len(set(layers)) != len(layers):
            raise ValueError("active layers must be unique values in 1..9: " +
                             profile_id)
        return layers

    resolved_patch_path(ORGAN_PATCH)
    genre_data = []
    for genre_id, patch_ids in genres.items():
        if not isinstance(patch_ids, list) or len(patch_ids) != 9:
            raise ValueError("genre must contain exactly nine layers: " + genre_id)
        if genre_id not in active_layers:
            raise ValueError("genre has no routing layer list: " + genre_id)
        layers = checked_layers(genre_id, active_layers[genre_id])
        for patch_id in patch_ids:
            resolved_patch_path(patch_id)
        genre_data.append((genre_id, patch_ids, layers))

    plugin_names = []
    indexes_by_patch = {}

    def add_patch_instance(patch_id):
        if patch_id not in indexes_by_patch:
            indexes_by_patch[patch_id] = len(plugin_names)
            plugin_names.append(patch_id)
        return indexes_by_patch[patch_id]

    organ_indexes = [0] * 9
    organ_indexes[0] = add_patch_instance(ORGAN_PATCH)
    profiles = [(ORGAN_PROFILE, 1, organ_indexes, [1]),
                (ORGAN_PROFILE, 3, organ_indexes, [1])]
    for pad_code, (genre_id, patch_ids, layers) in enumerate(genre_data, start=4):
        indexes = [0] * 9
        for layer in layers:
            indexes[layer - 1] = add_patch_instance(patch_ids[layer - 1])
        profiles.append((genre_id, pad_code, indexes, layers))
    plugin_paths = [resolved_patch_path(patch_id) for patch_id in plugin_names]
    return plugin_names, plugin_paths, profiles
