/*BHEADER**********************************************************************
 * (c) 2000   The Regents of the University of California
 *
 * See the file COPYRIGHT_and_DISCLAIMER for a complete copyright
 * notice, contact person, and disclaimer.
 *
 * $Revision$
 *********************************************************************EHEADER*/
/******************************************************************************
 *
 * HYPRE_SStructGraph interface
 *
 *****************************************************************************/

#include "headers.h"

/*--------------------------------------------------------------------------
 * HYPRE_SStructGraphCreate
 *--------------------------------------------------------------------------*/

int
HYPRE_SStructGraphCreate( MPI_Comm             comm,
                          HYPRE_SStructGrid    grid,
                          HYPRE_SStructGraph  *graph_ptr )
{
   int  ierr = 0;

   hypre_SStructGraph     *graph;
   int                     nparts;
   hypre_SStructStencil ***stencils;
   hypre_SStructPGrid    **pgrids;
   int                     nvars;
   int                     part, var;

   graph = hypre_TAlloc(hypre_SStructGraph, 1);

   hypre_SStructGraphComm(graph) = comm;
   hypre_SStructGraphNDim(graph) = hypre_SStructGridNDim(grid);
   hypre_SStructGridRef(grid, &hypre_SStructGraphGrid(graph));
   nparts = hypre_SStructGridNParts(grid);
   hypre_SStructGraphNParts(graph) = nparts;
   pgrids = hypre_SStructGridPGrids(grid);
   hypre_SStructGraphPGrids(graph) = pgrids;
   stencils = hypre_TAlloc(hypre_SStructStencil **, nparts);
   for (part = 0; part < nparts; part++)
   {
      nvars = hypre_SStructPGridNVars(pgrids[part]);
      stencils[part] = hypre_TAlloc(hypre_SStructStencil *, nvars);
      for (var = 0; var < nvars; var++)
      {
         stencils[part][var] = NULL;
      }
   }
   hypre_SStructGraphStencils(graph) = stencils;

   hypre_SStructGraphNUVEntries(graph) = 0;
   hypre_SStructGraphIUVEntries(graph) =
      hypre_CTAlloc(int, hypre_SStructGridLocalSize(grid));
   hypre_SStructGraphUVEntries(graph) =
      hypre_CTAlloc(hypre_SStructUVEntry *, hypre_SStructGridLocalSize(grid));
   hypre_SStructGraphRefCount(graph)   = 1;

   *graph_ptr = graph;

   return ierr;
}

/*--------------------------------------------------------------------------
 * HYPRE_SStructGraphDestroy
 *--------------------------------------------------------------------------*/

int
HYPRE_SStructGraphDestroy( HYPRE_SStructGraph graph )
{
   int  ierr = 0;

   int                     nparts;
   hypre_SStructPGrid    **pgrids;
   hypre_SStructStencil ***stencils;
   int                     nUventries;
   int                    *iUventries;
   hypre_SStructUVEntry  **Uventries;
   hypre_SStructUVEntry   *Uventry;
   int                     nvars;
   int                     part, var, i;

   if (graph)
   {
      hypre_SStructGraphRefCount(graph) --;
      if (hypre_SStructGraphRefCount(graph) == 0)
      {
         nparts   = hypre_SStructGraphNParts(graph);
         pgrids   = hypre_SStructGraphPGrids(graph);
         stencils = hypre_SStructGraphStencils(graph);
         nUventries = hypre_SStructGraphNUVEntries(graph);
         iUventries = hypre_SStructGraphIUVEntries(graph);
         Uventries  = hypre_SStructGraphUVEntries(graph);
         HYPRE_SStructGridDestroy(hypre_SStructGraphGrid(graph));
         for (part = 0; part < nparts; part++)
         {
            nvars = hypre_SStructPGridNVars(pgrids[part]);
            for (var = 0; var < nvars; var++)
            {
               HYPRE_SStructStencilDestroy(stencils[part][var]);
            }
            hypre_TFree(stencils[part]);
         }
         hypre_TFree(stencils);
         for (i = 0; i < nUventries; i++)
         {
            Uventry = Uventries[iUventries[i]];
            if (Uventry)
            {
               hypre_TFree(hypre_SStructUVEntryUEntries(Uventry));
               hypre_TFree(Uventry);
            }
            Uventries[iUventries[i]] = NULL;
         }
         hypre_TFree(iUventries);
         hypre_TFree(Uventries);
         hypre_TFree(graph);
      }
   }

   return ierr;
}

/*--------------------------------------------------------------------------
 * HYPRE_SStructGraphSetStencil
 *--------------------------------------------------------------------------*/

int
HYPRE_SStructGraphSetStencil( HYPRE_SStructGraph   graph,
                              int                  part,
                              int                  var,
                              HYPRE_SStructStencil stencil )
{
   int  ierr = 0;

   hypre_SStructStencilRef(stencil,
                           &hypre_SStructGraphStencil(graph, part, var));

   return ierr;
}

/*--------------------------------------------------------------------------
 * HYPRE_SStructGraphAddEntries
 *--------------------------------------------------------------------------*/

int
HYPRE_SStructGraphAddEntries( HYPRE_SStructGraph   graph,
                              int                  part,
                              int                 *index,
                              int                  var,
                              int                  nentries,
                              int                  to_part,
                              int                **to_indexes,
                              int                  to_var )
{
   int  ierr = 0;

   hypre_SStructGrid     *grid       = hypre_SStructGraphGrid(graph);
   int                    ndim       = hypre_SStructGridNDim(grid);
   int                    nUventries = hypre_SStructGraphNUVEntries(graph);
   int                   *iUventries = hypre_SStructGraphIUVEntries(graph);
   hypre_SStructUVEntry **Uventries  = hypre_SStructGraphUVEntries(graph);
   hypre_SStructUVEntry  *Uventry;
   int                    nUentries;
   hypre_SStructUEntry   *Uentries;

   hypre_Index            cindex;
   int                    box, rank;
   int                    i, j, first;

   /* compute location (rank) for Uventry */
   hypre_CopyToCleanIndex(index, ndim, cindex);
   hypre_SStructGridIndexToBox(grid, part, cindex, var, &box);
   hypre_SStructGridSVarIndexToRank(grid, box, part, cindex, var, &rank);
   rank -= hypre_SStructGridStartRank(grid);

   iUventries[nUventries] = rank;

   if (Uventries[rank] == NULL)
   {
      Uventry = hypre_TAlloc(hypre_SStructUVEntry, 1);
      hypre_SStructUVEntryPart(Uventry) = part;
      hypre_CopyToCleanIndex(index, ndim, hypre_SStructUVEntryIndex(Uventry));
      hypre_SStructUVEntryVar(Uventry) = var;
      first = 0;
      nUentries = nentries;
      Uentries = hypre_TAlloc(hypre_SStructUEntry, nUentries);
   }
   else
   {
      Uventry = Uventries[rank];
      first = hypre_SStructUVEntryNUEntries(Uventry);
      nUentries = first + nentries;
      Uentries = hypre_SStructUVEntryUEntries(Uventry);
      Uentries = hypre_TReAlloc(Uentries, hypre_SStructUEntry, nUentries);
   }
   hypre_SStructUVEntryNUEntries(Uventry) = nUentries;
   hypre_SStructUVEntryUEntries(Uventry)  = Uentries;

   for (i = 0; i < nentries; i++)
   {
      j = first + i;
      hypre_SStructUVEntryToPart(Uventry, j) = to_part;
      hypre_CopyToCleanIndex(to_indexes[i], ndim,
                             hypre_SStructUVEntryToIndex(Uventry, j));
      hypre_SStructUVEntryToVar(Uventry, j) = to_var;
   }

   Uventries[rank] = Uventry;

   hypre_SStructGraphNUVEntries(graph) ++;
   hypre_SStructGraphUVEntries(graph) = Uventries;

   return ierr;
}

/*--------------------------------------------------------------------------
 * HYPRE_SStructGraphAssemble
 *
 * TODO: Some of these ranks will need to be computed off-processor.
 *--------------------------------------------------------------------------*/

int
HYPRE_SStructGraphAssemble( HYPRE_SStructGraph graph )
{
   int ierr = 0;

   hypre_SStructGrid     *grid       = hypre_SStructGraphGrid(graph);
   int                    nUventries = hypre_SStructGraphNUVEntries(graph);
   int                   *iUventries = hypre_SStructGraphIUVEntries(graph);
   hypre_SStructUVEntry **Uventries  = hypre_SStructGraphUVEntries(graph);
   hypre_SStructUVEntry  *Uventry;
   int                    nUentries;
   int                    to_part;
   hypre_IndexRef         to_index;
   int                    to_var;
   int                    box, rank;
   int                    i, j;


   for (i = 0; i < nUventries; i++)
   {
      Uventry = Uventries[iUventries[i]];
      nUentries = hypre_SStructUVEntryNUEntries(Uventry);
      for (j = 0; j < nUentries; j++)
      {
         to_part  = hypre_SStructUVEntryToPart(Uventry, j);
         to_index = hypre_SStructUVEntryToIndex(Uventry, j);
         to_var   = hypre_SStructUVEntryToVar(Uventry, j);
         hypre_SStructGridIndexToBox(grid, to_part, to_index, to_var, &box);
         hypre_SStructGridSVarIndexToRank(grid, box, to_part, to_index, to_var,
                                          &rank);
         hypre_SStructUVEntryRank(Uventry, j) = rank;
      }
   }

   return ierr;
}
