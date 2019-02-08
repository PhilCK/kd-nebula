#include <karbon/drive.h>
#include <karbon/app.h>
#include <karbon/math.h>

#include <nebula/core.h>
#include <nebula/sugar.h>
#include <nebula/renderer.h>

#include <GL/gl3w.h>

#include <assert.h>

/* ----------------------------------------------------------- Application -- */


struct neb_sugar {
        /* nebula contexts */
        nbc_ctx_t core_ctx;
        nbr_ctx_t rdr_ctx;
        nbs_ctx_t sugar_ctx;
} nb;



void
think() {
        /* clear screen */
        glClearColor(1,1,0,1);
        glClear(GL_COLOR_BUFFER_BIT);
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
        
        nb_ok = nbc_ctx_create(&nb.core_ctx);
        assert(nb_ok == NB_OK);

        nb_ok = nbr_ctx_create(&nb.rdr_ctx);
        assert(nb_ok == NB_OK);

        nb_ok = nbs_ctx_create(&nb.sugar_ctx);
        assert(nb_ok == NB_OK);
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

        if(nb.rdr_ctx) {
                nb_ok = nbr_ctx_destroy(&nb.rdr_ctx);
                assert(nb_ok == NB_OK);
        }

        if(nb.core_ctx) {
                nb_ok = nbc_ctx_destroy(&nb.core_ctx);
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
