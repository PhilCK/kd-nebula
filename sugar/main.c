#include <karbon/drive.h>
#include <karbon/app.h>
#include <karbon/math.h>

#include <nebula/core.h>
#include <nebula/sugar.h>
#include <nebula/renderer.h>

#include <GL/gl3w.h>

#include <assert.h>

/* ----------------------------------------------------------- Application -- */


struct nb_gl {
        GLuint ftex[NB_FONT_COUNT_MAX];
        GLuint vao;
        GLuint pro;
        GLuint vbo, ibo;
        GLint unitex, uniproj;
        GLint inpos, intex, incol;
} nbgl;


struct neb_sugar {
        nbs_ctx_t sugar_ctx;
        nbr_ctx_t rdr_ctx;
        nbc_ctx_t core_ctx;
        
        struct nb_gl gl;
} nb;


void
nb_state_from_kd(nbc_ctx_t nb_core, uint64_t kd_events) {
        if(kd_events & KD_EVENT_INPUT_MS) {
                kd_result kd_ok = KD_RESULT_OK;
                nb_result nb_ok = NB_OK;

                struct kd_mouse_desc ms_desc;
                kd_ok = kd_input_get_mice(&ms_desc);
                assert(kd_ok == KD_RESULT_OK && "Failed to get KD Mouse");
                assert(ms_desc.ms_count >= 1 && "No mouse input");

                struct ms_state *ms = &ms_desc.ms_state[0];

                struct nb_pointer_desc ptr_desc;
                ptr_desc.x = ms->x;
                ptr_desc.y = ms->y;
                ptr_desc.interact = ms->keys[KD_MS_LEFT] & KD_KEY_DOWN ? 1 : 0;
                ptr_desc.scroll_y = 0;

                nb_ok = nbc_state_set_pointer(nb_core, &ptr_desc);
                assert(nb_ok == NB_OK && "Failed to set ptr desc");
        }

        if (kd_events & KD_EVENT_VIEWPORT_RESIZE) {
                kd_result kd_ok = KD_RESULT_OK;
                nb_result nb_ok = NB_OK;

                struct kd_window_desc win_desc;
                win_desc.type_id = KD_STRUCT_WINDOW_DESC;
                kd_ok = kd_window_get(&win_desc);
                assert(kd_ok == KD_RESULT_OK && "Failed to get KD Window");

                struct nb_viewport_desc view_desc;
                view_desc.width = win_desc.width;
                view_desc.height = win_desc.height;

                nb_ok = nbc_state_set_viewport(nb_core, &view_desc);
                assert(nb_ok == NB_OK && "Failed to set view desc");
        }
}


void
nb_render_to_gl(nbr_ctx_t nb_rdr, struct nb_gl *gl, int vp_width, int vp_height) {
        (void)nb_rdr;

        /* clear screen */
        glClearColor(1,1,0,1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        /* get nebula render data */
        struct nb_render_data rd;
        memset(&rd, 0, sizeof(rd));
        nb_get_render_data(nb_rdr, &rd);

        /* setup gl */
        glDisable(GL_DEPTH_TEST);

        /* prepare pass */
        GLfloat proj[4][4] = {
                { 2.f, 0.f, 0.f, 0.f },
                { 0.f, -2.f, 0.f, 0.f },
                { 0.f, 0.f, -1.f, 0.f },
                { -1.f, 1.f, 0.f, 1.f },
        };

        proj[0][0] /= (GLfloat)vp_width;
        proj[1][1] /= (GLfloat)vp_height;

        glUseProgram(gl->pro);
        glUniformMatrix4fv(gl->uniproj, 1, GL_FALSE, &proj[0][0]);
        glViewport(0, 0, vp_width, vp_height);

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

        unsigned int f_idx = nb_debug_get_font(nb_rdr);
        GLuint ftex = gl->ftex[f_idx];

        glUniform1i(gl->unitex, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, ftex);

        glBindVertexArray(gl->vao);
        glBindBuffer(GL_ARRAY_BUFFER, gl->vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl->ibo);

        int vtx_size = sizeof(rd.vtx[0]) * rd.vtx_count;
        int idx_size = sizeof(rd.idx[0]) * rd.idx_count;

        glBufferData(GL_ARRAY_BUFFER, vtx_size, NULL, GL_STREAM_DRAW);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx_size, NULL, GL_STREAM_DRAW);

        void *vbo_data = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
        void *ibo_data = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);

        memcpy(vbo_data, (void*)rd.vtx, vtx_size);
        memcpy(ibo_data, (void*)rd.idx, idx_size);

        glUnmapBuffer(GL_ARRAY_BUFFER);
        glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);

        glEnable(GL_SCISSOR_TEST);

        /* render */
        for (unsigned int list_idx = 0; list_idx < rd.cmd_list_count; list_idx++) {
                struct nb_render_cmd_list * cmd_list = rd.cmd_lists + list_idx;

                for (unsigned int i = 0; i < cmd_list->count; ++i) {
                        struct nb_render_cmd * cmd = cmd_list->cmds + i;

                        if (cmd->type == NB_RENDER_CMD_TYPE_SCISSOR) {
                                int w = cmd->data.clip_rect[2];
                                int h = cmd->data.clip_rect[3];

                                int x = cmd->data.clip_rect[0];
                                int y = vp_height - (cmd->data.clip_rect[1] + h);

                                glScissor(x, y, w, h);
                        }
                        else {
                                GLenum mode = GL_TRIANGLES;

                                if (cmd->type == NB_RENDER_CMD_TYPE_LINES) {
                                        mode = GL_LINE_STRIP;
                                }

                                unsigned long offset = cmd->data.elem.offset * sizeof(unsigned short);
                                glDrawElements(
                                        mode,
                                        cmd->data.elem.count,
                                        GL_UNSIGNED_SHORT,
                                        (void *)offset);
                        }
                }
        }

        glDisable(GL_SCISSOR_TEST);

        GLuint err = glGetError();
        assert(err == 0 && "Failed GL draw");
}


void
think() {
        /* KD State */
        kd_result kd_ok = KD_RESULT_OK;
        uint64_t kd_events = 0;
        kd_ok = kd_events_get(&kd_events);
        assert(kd_ok == KD_RESULT_OK && "Failed to get KD events");

        /* Nebula Frame */
        nb_result nb_ok = NB_OK;

        /* Nebula State */
        nb_state_from_kd(nb.core_ctx, kd_events);

        /* Nebula Demo */
        nb_ok = nbs_frame_begin(nb.sugar_ctx);
        assert(nb_ok == NB_OK && "Failed to start frame");

        const struct nb_window *win = 0;
        win = nbs_window_begin(nb.sugar_ctx, "Test Window", 0xFF0000FF);
        nbs_window_end(nb.sugar_ctx, win);

        nb_ok = nbs_frame_submit(nb.sugar_ctx);
        assert(nb_ok == NB_OK && "Failed to submit frame");

        /* Nebula Render */
        struct nb_state st;
        nbc_state_get(nb.core_ctx, &st);
        nb_render_to_gl(nb.rdr_ctx, &nb.gl, st.vp_width, st.vp_height);
}


void
nb_gl_setup(nbr_ctx_t nbr_ctx, struct nb_gl *nb_gl) {
        unsigned int font_count = nb_get_font_count(nbr_ctx);
        assert(font_count);

        unsigned int i;

        struct nb_font_tex font_tex_list[NB_FONT_COUNT_MAX];
        nb_get_font_tex_list(nbr_ctx, font_tex_list);
        for (i = 0; i < font_count; i++) {
                struct nb_font_tex * f = font_tex_list + i;

                GLuint * ftex = nb_gl->ftex + i;
                glGenTextures(1, ftex);
                assert(*ftex);
                glBindTexture(GL_TEXTURE_2D, *ftex);
                glTexImage2D(
                        GL_TEXTURE_2D,
                        0,
                        GL_R8,
                        f->width,
                        f->width,
                        0,
                        GL_RED,
                        GL_UNSIGNED_BYTE,
                        f->mem);

                glTexParameteri(
                        GL_TEXTURE_2D,
                        GL_TEXTURE_MIN_FILTER,
                        GL_LINEAR);
        }

        const char * vs_src = "#version 150\n"
                "in vec2 position;\n"
                "in vec2 texcoord;\n"
                "in vec4 color;\n"
                "uniform mat4 projection;\n"
                "out vec2 frag_uv;\n"
                "out vec4 frag_color;\n"
                "void main() {"
                "       frag_uv = texcoord;\n"
                "       frag_color = color;\n"
                "       gl_Position = projection * vec4(position.xy, 0, 1);\n"
                "}";

        const char * fs_src = "#version 150\n"
                "in vec2 frag_uv;\n"
                "in vec4 frag_color;\n"
                "uniform sampler2D texture_map;\n"
                "out vec4 out_color;\n"
                "void main() {"
                "       float a = frag_color.a;"
                "       if(frag_uv.x != 0.0 || frag_uv.y != 0.0) {"
                "               a *= texture(texture_map, frag_uv.xy).r;"
                "       }"
                "       out_color = vec4(frag_color.rgb * a, a);\n"
                "}";

        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(vs, 1, (const GLchar * const *)&vs_src, 0);
        glShaderSource(fs, 1, (const GLchar * const *)&fs_src, 0);

        glCompileShader(vs);
        glCompileShader(fs);

        GLint status;
        glGetShaderiv(vs, GL_COMPILE_STATUS, &status);
        assert(status == GL_TRUE);
        glGetShaderiv(fs, GL_COMPILE_STATUS, &status);
        assert(status == GL_TRUE);

        nb_gl->pro = glCreateProgram();
        glAttachShader(nb_gl->pro, vs);
        glAttachShader(nb_gl->pro, fs);
        glLinkProgram(nb_gl->pro);
        glGetProgramiv(nb_gl->pro, GL_LINK_STATUS, &status);
        assert(status == GL_TRUE);

        glDeleteShader(vs);
        glDeleteShader(fs);

        nb_gl->unitex = glGetUniformLocation(nb_gl->pro, "texture_map");
        nb_gl->uniproj = glGetUniformLocation(nb_gl->pro, "projection");
        nb_gl->inpos = glGetAttribLocation(nb_gl->pro, "position");
        nb_gl->incol = glGetAttribLocation(nb_gl->pro, "color");
        nb_gl->intex = glGetAttribLocation(nb_gl->pro, "texcoord");

        glGenBuffers(1, &nb_gl->ibo);
        glGenBuffers(1, &nb_gl->vbo);
        glGenVertexArrays(1, &nb_gl->vao);

        glBindVertexArray(nb_gl->vao);
        glBindBuffer(GL_ARRAY_BUFFER, nb_gl->vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, nb_gl->ibo);

        glEnableVertexAttribArray(nb_gl->inpos);
        glEnableVertexAttribArray(nb_gl->incol);
        glEnableVertexAttribArray(nb_gl->intex);

        struct vertex {
                float pos[2];
                float uv[2];
                float color[4];
        };

        GLsizei stride = sizeof(struct vertex);
        void * ptr = (void*)offsetof(struct vertex, pos);
        glVertexAttribPointer(nb_gl->inpos, 2, GL_FLOAT, GL_FALSE, stride, ptr);

        ptr = (void*)offsetof(struct vertex, uv);
        glVertexAttribPointer(nb_gl->intex, 2, GL_FLOAT, GL_FALSE, stride, ptr);

        ptr = (void*)offsetof(struct vertex, color);
        glVertexAttribPointer(nb_gl->incol, 4, GL_FLOAT, GL_FALSE, stride, ptr);

        GLuint err = glGetError();
        assert(err == 0 && "Failed GL setup");
}


void
setup() {
        memset(&nb, 0, sizeof(nb));

        /* Log out startup */
        kd_result kd_ok = KD_RESULT_OK;
        kd_ok = kd_log(KD_LOG_INFO, "Nebula Sugar Setup");
        assert(kd_ok == KD_RESULT_OK);

        /* Setup OpenGL */
        kd_ok = kd_gl_make_current();
        assert(kd_ok == KD_RESULT_OK);

        if (gl3wInit()) {
                kd_log(KD_LOG_FATAL, "Failed to init OpenGL");
                assert(!"Failed to init OpenGL");

                kd_ctx_close();
                return;
        }

        /* Setup Nebula ctx's */
        nb_result nb_ok = NB_OK;
        nb_ok = nbs_ctx_create(&nb.sugar_ctx);
        assert(nb_ok == NB_OK && nb.sugar_ctx && "Failed to setup Neb");

        nb_ok = nbs_ctx_get_ctx(nb.sugar_ctx, &nb.core_ctx, &nb.rdr_ctx);
        assert(nb_ok == NB_OK && "Failed to get CTX");
        assert(nb.core_ctx && "No Core ctx");
        assert(nb.rdr_ctx && "No Renderer ctx");

        /* Setup GL */
        nb_gl_setup(nb.rdr_ctx, &nb.gl);
}


void
shutdown() {
        /* Log out shutdown */
        kd_result kd_ok = KD_RESULT_OK;
        kd_ok = kd_log(KD_LOG_INFO, "Nebula Sugar Shutdown");
        assert(kd_ok == KD_RESULT_OK);

        /* Destroy Nebula ctx's */
        nb_result nb_ok = NB_OK;

        if(nb.sugar_ctx) {
                nb_ok = nbs_ctx_destroy(&nb.sugar_ctx);
                assert(nb_ok == NB_OK);
        }

        /* Destroy OpenGL */
}


/* ----------------------------------------------- Application Description -- */


KD_APP_NAME("Neb-Sugar")
KD_APP_DESC("Nebula's sugar test")
KD_APP_GRAPHICS_API("OpenGL")
KD_APP_STARTUP_FN(setup)
KD_APP_TICK_FN(think)
KD_APP_SHUTDOWN_FN(shutdown)
