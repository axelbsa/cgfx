#include "cgfx_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum Section
{
    SECTION_NONE,
    SECTION_POINTS,
    SECTION_INDICES
};

static void strip_cr(char *line) {
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') line[--len] = '\0';
    if (len > 0 && line[len - 1] == '\r') line[--len] = '\0';
}

bool cgfx_load_geometry(const char *path, CgfxGeometry *out) {
    FILE *f = fopen(path, "r");
    if (!f) return false;

    *out = (CgfxGeometry){};

    uint32_t point_cap = 0;
    uint32_t index_cap = 0;
    enum Section section = SECTION_NONE;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        strip_cr(line);

        if (strcmp(line, "[points]") == 0) {
            section = SECTION_POINTS;
        } else if (strcmp(line, "[indices]") == 0) {
            section = SECTION_INDICES;
        } else if (line[0] == '#' || line[0] == '\0') {
            continue;
        } else if (section == SECTION_POINTS) {
            float v[6];
            int n = sscanf(line, "%f %f %f %f %f %f",
                           &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]);
            if (n < 5) continue;
            if (out->floats_per_point == 0) out->floats_per_point = (uint32_t)n;
            if (out->point_count + (uint32_t)n > point_cap) {
                point_cap = point_cap ? point_cap * 2 : 64;
                out->point_data = realloc(out->point_data, point_cap * sizeof(float));
            }
            memcpy(&out->point_data[out->point_count], v, (uint32_t)n * sizeof(float));
            out->point_count += (uint32_t)n;
        } else if (section == SECTION_INDICES) {
            unsigned i0, i1, i2;
            if (sscanf(line, "%u %u %u", &i0, &i1, &i2) != 3)
                continue;
            if (out->index_count + 3 > index_cap) {
                index_cap = index_cap ? index_cap * 2 : 64;
                out->index_data = realloc(out->index_data, index_cap * sizeof(uint16_t));
            }
            out->index_data[out->index_count++] = (uint16_t)i0;
            out->index_data[out->index_count++] = (uint16_t)i1;
            out->index_data[out->index_count++] = (uint16_t)i2;
        }
    }

    fclose(f);
    return true;
}

void cgfx_free_geometry(CgfxGeometry *geo) {
    free(geo->point_data);
    free(geo->index_data);
    *geo = (CgfxGeometry){};
}

CgfxMesh cgfx_load_tutorial_mesh(const CgfxCtx *ctx, const char *path) {
    CgfxGeometry geo;
    if (!cgfx_load_geometry(path, &geo)) {
        fprintf(stderr, "[cgfx] Failed to load geometry: %s\n", path);
        return (CgfxMesh){};
    }

    uint32_t fpp = geo.floats_per_point;
    uint32_t vertex_count = geo.point_count / fpp;
    bool has_z = (fpp >= 6);

    CgfxVertex *vertices = malloc(vertex_count * sizeof(CgfxVertex));
    for (uint32_t i = 0; i < vertex_count; i++) {
        const float *p = &geo.point_data[i * fpp];
        if (has_z) {
            vertices[i] = (CgfxVertex){
                .position = {p[0], p[1], p[2]},
                .color    = {p[3], p[4], p[5], 1.0f},
            };
        } else {
            vertices[i] = (CgfxVertex){
                .position = {p[0], p[1], 0.0f},
                .color    = {p[2], p[3], p[4], 1.0f},
            };
        }
    }

    uint32_t *indices = malloc(geo.index_count * sizeof(uint32_t));
    for (uint32_t i = 0; i < geo.index_count; i++)
        indices[i] = geo.index_data[i];

    CgfxMesh mesh = cgfx_mesh_create(ctx, vertices, vertex_count, indices, geo.index_count);

    free(vertices);
    free(indices);
    cgfx_free_geometry(&geo);

    return mesh;
}
