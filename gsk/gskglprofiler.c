#include "config.h"

#include "gskglprofilerprivate.h"

#include <epoxy/gl.h>

#define N_QUERIES       4

struct _GskGLProfiler
{
  GObject parent_instance;

  /* Creating GL queries is kind of expensive, so we pay the
   * price upfront and create a circular buffer of queries
   */
  GLuint gl_queries[N_QUERIES];
  GLuint active_query;

  gboolean first_frame : 1;
};

G_DEFINE_TYPE (GskGLProfiler, gsk_gl_profiler, G_TYPE_OBJECT)

static void
gsk_gl_profiler_finalize (GObject *gobject)
{
  GskGLProfiler *self = GSK_GL_PROFILER (gobject);

  glDeleteQueries (N_QUERIES, &self->gl_queries[0]);

  G_OBJECT_CLASS (gsk_gl_profiler_parent_class)->finalize (gobject);
}

static void
gsk_gl_profiler_class_init (GskGLProfilerClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = gsk_gl_profiler_finalize;
}

static void
gsk_gl_profiler_init (GskGLProfiler *self)
{
  glGenQueries (N_QUERIES, &self->gl_queries[0]);

  self->first_frame = TRUE;
}

GskGLProfiler *
gsk_gl_profiler_new (void)
{
  return g_object_new (GSK_TYPE_GL_PROFILER, NULL);
}

void
gsk_gl_profiler_begin_gpu_region (GskGLProfiler *profiler)
{
  GLuint query_id;

  g_return_if_fail (GSK_IS_GL_PROFILER (profiler));

  query_id = profiler->gl_queries[profiler->active_query];
  glBeginQuery (GL_TIME_ELAPSED, query_id);
}

guint64
gsk_gl_profiler_end_gpu_region (GskGLProfiler *profiler)
{
  GLuint last_query_id;
  GLint res;
  GLuint64 elapsed;

  g_return_val_if_fail (GSK_IS_GL_PROFILER (profiler), 0);

  glEndQuery (GL_TIME_ELAPSED);

  if (profiler->active_query == 0)
    last_query_id = N_QUERIES - 1;
  else
    last_query_id = profiler->active_query - 1;

  /* Advance iterator */
  profiler->active_query += 1;
  if (profiler->active_query == N_QUERIES)
    profiler->active_query = 0;

  /* If this is the first frame we already have a result */
  if (profiler->first_frame)
    {
      profiler->first_frame = FALSE;
      return 0;
    }

  glGetQueryObjectiv (last_query_id, GL_QUERY_RESULT_AVAILABLE, &res);
  if (res == 1)
    glGetQueryObjectui64v (last_query_id, GL_QUERY_RESULT, &elapsed);
  else
    elapsed = 0;

  return elapsed;
}
