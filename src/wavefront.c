/* Goxel 3D voxels editor
 *
 * copyright (c) 2015 Guillaume Chereau <guillaume@noctua-software.com>
 *
 * Goxel is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.

 * Goxel is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.

 * You should have received a copy of the GNU General Public License along with
 * goxel.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "goxel.h"

typedef struct {
    char    type[2];
    union {
        struct {
            vec3_t   v;
            uvec3b_t c;
        };
        vec3_t vn;
        struct {
            int  vs[4];
            int vns[4];
        };
    };
} line_t;

static UT_icd line_icd = {sizeof(line_t), NULL, NULL, NULL};

static int lines_find(UT_array *lines, const line_t *line)
{
    int i, ret = 0;
    line_t *l;
    for (i = 0; i < utarray_len(lines); i++) {
        l = (line_t*)utarray_eltptr(lines, i);
        if (strncmp(l->type, line->type, 2) != 0) continue;
        ret++;
        if (memcmp(l, line, sizeof(*line)) == 0)
            return ret;
    }
    return 0;
}

static int lines_add(UT_array *lines, const line_t *line)
{
    int i;
    i = lines_find(lines, line);
    if (i) return i;
    utarray_push_back(lines, line);
    return lines_find(lines, line);
}

static int lines_count(UT_array *lines, const char *type)
{
    line_t *line = NULL;
    int ret = 0;
    while( (line = (line_t*)utarray_next(lines, line))) {
        if (strncmp(line->type, type, 2) == 0)
            ret++;
    }
    return ret;
}

static int line_cmp_score(const line_t *line)
{
    if (line->type[0] == 'v' && line->type[1] == ' ') return 0;
    if (line->type[0] == 'v' && line->type[1] == 'n') return 1;
    if (line->type[0] == 'f' && line->type[1] == ' ') return 2;
    assert(false);
    return 0;
}

static int line_cmp(const void *a_, const void *b_)
{
    const line_t *a = a_;
    const line_t *b = b_;
    return sign(line_cmp_score(a) - line_cmp_score(b));
}

void wavefront_export(const mesh_t *mesh, const char *path)
{
    // XXX: Merge faces that can be merged into bigger ones.
    //      Allow to chose between quads or triangles.
    //      Also export mlt file for the colors.
    block_t *block;
    voxel_vertex_t* verts;
    vec3_t v;
    int nb_quads, i, j;
    mat4_t mat;
    FILE *out;
    const int N = BLOCK_SIZE;
    UT_array *lines;
    line_t line, face, *line_ptr;

    utarray_new(lines, &line_icd);
    verts = calloc(N * N * N * 6 * 4, sizeof(*verts));
    face = (line_t){"f "};
    DL_FOREACH(mesh->blocks, block) {
        mat = mat4_identity;
        mat4_itranslate(&mat, block->pos.x, block->pos.y, block->pos.z);
        mat4_itranslate(&mat, -N / 2 + 0.5, -N / 2 + 0.5, -N / 2 + 0.5);

        nb_quads = block_generate_vertices(block->data, 0, verts);
        for (i = 0; i < nb_quads; i++) {
            // Put the vertices.
            for (j = 0; j < 4; j++) {
                v = vec3(verts[i * 4 + j].pos.x,
                         verts[i * 4 + j].pos.y,
                         verts[i * 4 + j].pos.z);
                v = mat4_mul_vec3(mat, v);
                line = (line_t){"v ", .v = v};
                face.vs[j] = lines_add(lines, &line);
            }
            // Put the normals
            for (j = 0; j < 4; j++) {
                v = vec3(verts[i * 4 + j].normal.x,
                         verts[i * 4 + j].normal.y,
                         verts[i * 4 + j].normal.z);
                line = (line_t){"vn", .vn = v};
                face.vns[j] = lines_add(lines, &line);
            }
            lines_add(lines, &face);
        }
    }
    utarray_sort(lines, line_cmp);
    out = fopen(path, "w");
    fprintf(out, "# Goxel " GOXEL_VERSION_STR "\n");
    line_ptr = NULL;
    while( (line_ptr = (line_t*)utarray_next(lines, line_ptr))) {
        if (strncmp(line_ptr->type, "v ", 2) == 0)
            fprintf(out, "v %g %g %g\n", VEC3_SPLIT(line_ptr->v));
        if (strncmp(line_ptr->type, "vn", 2) == 0)
            fprintf(out, "vn %g %g %g\n", VEC3_SPLIT(line_ptr->vn));
        if (strncmp(line_ptr->type, "f ", 2) == 0)
            fprintf(out, "f %d//%d %d//%d %d//%d %d//%d\n",
                         line_ptr->vs[0], line_ptr->vns[0],
                         line_ptr->vs[1], line_ptr->vns[1],
                         line_ptr->vs[2], line_ptr->vns[2],
                         line_ptr->vs[3], line_ptr->vns[3]);
    }
    fclose(out);
    utarray_free(lines);
}

void ply_export(const mesh_t *mesh, const char *path)
{
    block_t *block;
    voxel_vertex_t* verts;
    vec3_t v;
    uvec3b_t c;
    int nb_quads, i, j;
    mat4_t mat;
    FILE *out;
    const int N = BLOCK_SIZE;
    UT_array *lines;
    line_t line, face, *line_ptr;

    utarray_new(lines, &line_icd);
    verts = calloc(N * N * N * 6 * 4, sizeof(*verts));
    face = (line_t){"f "};
    DL_FOREACH(mesh->blocks, block) {
        mat = mat4_identity;
        mat4_itranslate(&mat, block->pos.x, block->pos.y, block->pos.z);
        mat4_itranslate(&mat, -N / 2 + 0.5, -N / 2 + 0.5, -N / 2 + 0.5);

        nb_quads = block_generate_vertices(block->data, 0, verts);
        for (i = 0; i < nb_quads; i++) {
            // Put the vertices.
            for (j = 0; j < 4; j++) {
                v = vec3(verts[i * 4 + j].pos.x,
                         verts[i * 4 + j].pos.y,
                         verts[i * 4 + j].pos.z);
                v = mat4_mul_vec3(mat, v);
                c = verts[i * 4 + j].color.rgb;
                line = (line_t){"v ", .v = v, .c = c};
                face.vs[j] = lines_add(lines, &line);
            }
            // Put the normals
            for (j = 0; j < 4; j++) {
                v = vec3(verts[i * 4 + j].normal.x,
                         verts[i * 4 + j].normal.y,
                         verts[i * 4 + j].normal.z);
                line = (line_t){"vn", .vn = v};
                face.vns[j] = lines_add(lines, &line);
            }
            lines_add(lines, &face);
        }
    }
    utarray_sort(lines, line_cmp);
    out = fopen(path, "w");
    fprintf(out, "ply\n");
    fprintf(out, "format ascii 1.0\n");
    fprintf(out, "comment Generated from Goxel " GOXEL_VERSION_STR "\n");
    fprintf(out, "element vertex %d\n", lines_count(lines, "v "));
    fprintf(out, "property float x\n");
    fprintf(out, "property float y\n");
    fprintf(out, "property float z\n");
    fprintf(out, "property uchar red\n");
    fprintf(out, "property uchar green\n");
    fprintf(out, "property uchar blue\n");
    fprintf(out, "element face %d\n", lines_count(lines, "f "));
    fprintf(out, "property list uchar int vertex_index\n");
    fprintf(out, "end_header\n");
    line_ptr = NULL;
    while( (line_ptr = (line_t*)utarray_next(lines, line_ptr))) {
        if (strncmp(line_ptr->type, "v ", 2) == 0)
            fprintf(out, "%g %g %g %d %d %d\n",
                    VEC3_SPLIT(line_ptr->v),
                    VEC3_SPLIT(line_ptr->c));
        if (strncmp(line_ptr->type, "f ", 2) == 0)
            fprintf(out, "4 %d %d %d %d\n", line_ptr->vs[0] - 1,
                                            line_ptr->vs[1] - 1,
                                            line_ptr->vs[2] - 1,
                                            line_ptr->vs[3] - 1);
    }
    fclose(out);
    utarray_free(lines);
}
