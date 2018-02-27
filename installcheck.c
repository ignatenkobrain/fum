/* SPDX-License-Identifier: GPL-3.0+ */

#include <solv/pool.h>
#include <solv/poolarch.h>
#include <solv/repo.h>
#include <solv/repo_solv.h>
#include <solv/solver.h>
#include <solv/solv_xfopen.h>

#define ARCH "x86_64"

int
main (int argc, char **argv)
{
  Pool *pool;
  Solver *solv;
  Repo *binary, *source;
  Queue cand, job;
  Id p;
  FILE *fp;

  pool = pool_create ();
  pool_setarch (pool, ARCH);

  binary = repo_create (pool, "tmp");
  fp = solv_xfopen ("/var/cache/dnf/tmp.solv", 0);
  repo_add_solv (binary, fp, 0);
  fp = solv_xfopen ("/var/cache/dnf/tmp-filenames.solvx", 0);
  repo_add_solv (binary, fp, REPO_EXTEND_SOLVABLES | REPO_LOCALPOOL);

  source = repo_create (pool, "tmp-src");
  fp = solv_xfopen ("/var/cache/dnf/tmp-src.solv", 0);
  repo_add_solv (source, fp, 0);

  pool_addfileprovides (pool);
  pool_createwhatprovides (pool);

  if (!pool->considered)
    {
      pool->considered = solv_calloc (1, sizeof (Map));
      map_init (pool->considered, pool->nsolvables);
      map_setall (pool->considered);
    }

  for (int i = 1; i < argc; i++)
    {
      const char *n = argv[i];
      Id nid = pool_str2id (pool, n, 0);
      if (!nid)
        fprintf (stderr, "WARNING: can't find %s, skipping\n", n);
      FOR_POOL_SOLVABLES (p)
        {
          Solvable *s = pool->solvables + p;
          if (s->name != nid)
            continue;
          fprintf (stderr, "DEBUG: disabling %s\n", pool_solvable2str (pool, s));
          MAPCLR (pool->considered, p);
        }
    }

  queue_init (&job);
  queue_init (&cand);
  for (p = 1; p < pool->nsolvables; p++)
    {
      Solvable *s = pool->solvables + p;
      if (!s->repo)
        continue;
      if (MAPTST (pool->considered, p) && (s->arch == ARCH_SRC || s->arch == ARCH_NOSRC))
        {
          queue_push (&cand, p);
          continue;
        }
      if (!pool_installable (pool, s))
        continue;
      queue_push (&cand, p);
    }

  solv = solver_create (pool);

  while (cand.count)
    {
      int i, j;

      queue_empty (&job);
      for (i = 0; i < cand.count; i++)
        {
          p = cand.elements[i];
          queue_push2 (&job, SOLVER_INSTALL | SOLVER_SOLVABLE | SOLVER_WEAK, p);
        }

      solver_solve (solv, &job);
      /* prune */
      for (i = j = 0; i < cand.count; i++)
        {
          p = cand.elements[i];
          if (solver_get_decisionlevel (solv, p) <= 0)
            {
              cand.elements[j++] = p;
              continue;
            }
        }
      cand.count = j;
      if (i == j)
        break;
    }

  for (int i = 0; i < cand.count; i++)
    {
      p = cand.elements[i];

      Solvable *s = pool->solvables + p;
      queue_empty (&job);
      queue_push2 (&job, SOLVER_INSTALL | SOLVER_SOLVABLE, p);

      int problemcount = solver_solve (solv, &job);
      if (problemcount)
        {
          Id problem = 0;
          Queue rids;
          queue_init (&rids);

          printf ("can't install %s\n", pool_solvable2str (pool, s));
          while ((problem = solver_next_problem (solv, problem)) != 0)
            {
              solver_findallproblemrules (solv, problem, &rids);
              for (int j = 0; j < rids.count; j++)
                {
                  Id probr = rids.elements[j];
                  Queue rinfo;
                  queue_init (&rinfo);

                  solver_allruleinfos (solv, probr, &rinfo);
                  for (int k = 0; k < rinfo.count; k += 4)
                    {
                      Id type, source, target, dep;
                      type = rinfo.elements[k];
                      source = rinfo.elements[k + 1];
                      target = rinfo.elements[k + 2];
                      dep = rinfo.elements[k + 3];
                      printf ("  - %s\n", solver_problemruleinfo2str(solv, type, source, target, dep));
                    }
                }
            }
        }
    }

  return 0;
}
