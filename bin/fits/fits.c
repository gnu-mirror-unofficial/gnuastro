/*********************************************************************
Fits - View and manipulate FITS extensions and/or headers.
Fits is part of GNU Astronomy Utilities (Gnuastro) package.

Original author:
     Mohammad Akhlaghi <mohammad@akhlaghi.org>
Contributing author(s):
Copyright (C) 2017-2020, Free Software Foundation, Inc.

Gnuastro is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation, either version 3 of the License, or (at your
option) any later version.

Gnuastro is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with Gnuastro. If not, see <http://www.gnu.org/licenses/>.
**********************************************************************/
#include <config.h>

#include <errno.h>
#include <error.h>
#include <string.h>
#include <unistd.h>

#include <gnuastro/wcs.h>
#include <gnuastro/list.h>
#include <gnuastro/fits.h>
#include <gnuastro/blank.h>
#include <gnuastro/pointer.h>
#include <gnuastro/statistics.h>

#include <gnuastro-internal/timing.h>
#include <gnuastro-internal/checkset.h>

#include "main.h"

#include "fits.h"
#include "keywords.h"



int
fits_has_error(struct fitsparams *p, int actioncode, char *string, int status)
{
  char *action=NULL;
  int r=EXIT_SUCCESS;

  switch(actioncode)
    {
    case FITS_ACTION_DELETE:        action="deleted";      break;
    case FITS_ACTION_RENAME:        action="renamed";      break;
    case FITS_ACTION_UPDATE:        action="updated";      break;
    case FITS_ACTION_WRITE:         action="written";      break;
    case FITS_ACTION_COPY:          action="copied";       break;
    case FITS_ACTION_REMOVE:        action="removed";      break;

    default:
      error(EXIT_FAILURE, 0, "%s: a bug! Please contact us at '%s' so we "
            "can fix this problem. The value of 'actioncode' must not be %d",
            __func__, PACKAGE_BUGREPORT, actioncode);
    }

  if(p->quitonerror)
    {
      fits_report_error(stderr, status);
      error(EXIT_FAILURE, 0, "%s: %s: not %s\n", __func__, string, action);
    }
  else
    {
      fprintf(stderr, "%s: Not %s.\n", string, action);
      r=EXIT_FAILURE;
    }
  return r;
}





/* Print all the extension informations. */
void
fits_print_extension_info(struct fitsparams *p)
{
  uint16_t *ui16;
  fitsfile *fptr;
  int hasblankname=0;
  gal_data_t *cols=NULL, *tmp;
  char **tstra, **estra, **sstra;
  size_t i, numext, *dsize, ndim;
  int j, nc, numhdu, hdutype, status=0, type;
  char *msg, *tstr=NULL, sstr[1000], extname[FLEN_VALUE];


  /* Open the FITS file and read the first extension type, upon moving to
     the next extension, we will read its type, so for the first we will
     need to do it explicitly. */
  fptr=gal_fits_hdu_open(p->filename, "0", READONLY);
  if (fits_get_hdu_type(fptr, &hdutype, &status) )
    gal_fits_io_error(status, "reading first extension");


  /* Get the number of HDUs. */
  if( fits_get_num_hdus(fptr, &numhdu, &status) )
    gal_fits_io_error(status, "finding number of HDUs");
  numext=numhdu;


  /* Allocate all the columns (in reverse order, since this is a simple
     linked list). */
  gal_list_data_add_alloc(&cols, NULL, GAL_TYPE_STRING, 1, &numext, NULL, 1,
                          -1, 1, "HDU_SIZE", "name", "Size of image or table "
                          "number of rows and columns.");
  gal_list_data_add_alloc(&cols, NULL, GAL_TYPE_STRING, 1, &numext, NULL, 1,
                          -1, 1, "HDU_TYPE", "name", "Image data type or "
                          "'table' format (ASCII or binary).");
  gal_list_data_add_alloc(&cols, NULL, GAL_TYPE_STRING, 1, &numext, NULL, 1,
                          -1, 1, "EXTNAME", "name", "Extension name of this "
                          "HDU (EXTNAME in FITS).");
  gal_list_data_add_alloc(&cols, NULL, GAL_TYPE_UINT16, 1, &numext, NULL, 1,
                          -1, 1, "HDU_INDEX", "count", "Index (starting from "
                          "zero) of each HDU (extension).");


  /* Keep pointers to the array of each column for easy writing. */
  ui16  = cols->array;
  estra = cols->next->array;
  tstra = cols->next->next->array;
  sstra = cols->next->next->next->array;

  cols->next->disp_width=15;
  cols->next->next->disp_width=15;


  /* Fill in each column. */
  for(i=0;i<numext;++i)
    {
      /* Work based on the type of the extension. */
      switch(hdutype)
        {
        case IMAGE_HDU:
          gal_fits_img_info(fptr, &type, &ndim, &dsize, NULL, NULL);
          tstr = ndim==0 ? "no-data" : gal_type_name(type , 1);
          break;

        case ASCII_TBL:
        case BINARY_TBL:
          ndim=2;
          tstr = hdutype==ASCII_TBL ? "table_ascii" : "table_binary";
          dsize=gal_pointer_allocate(GAL_TYPE_SIZE_T, 2, 0, __func__,
                                     "dsize");
          gal_fits_tab_size(fptr, dsize+1, dsize);
          break;

        default:
          error(EXIT_FAILURE, 0, "%s: a bug! the 'hdutype' code %d not "
                "recognized", __func__, hdutype);
        }


      /* Read the extension name*/
      fits_read_keyword(fptr, "EXTNAME", extname, NULL, &status);
      switch(status)
        {
        case 0:
          gal_fits_key_clean_str_value(extname);
          break;

        case KEY_NO_EXIST:
          sprintf(extname, "%s", GAL_BLANK_STRING);
          hasblankname=1;
          status=0;
          break;

        default:
          gal_fits_io_error(status, "reading EXTNAME keyword");
        }
      status=0;


      /* Write the size into a string. 'sprintf' returns the number of
         written characters (excluding the '\0'). So for each dimension's
         size that is written, we add to 'nc' (the number of
         characters). Note that FITS allows blank extensions, in those
         cases, return "0". */
      if(ndim>0)
        {
          nc=0;
          for(j=ndim-1;j>=0;--j)
            nc += sprintf(sstr+nc, "%zux", dsize[j]);
          sstr[nc-1]='\0';
          free(dsize);
        }
      else
        {
          sstr[0]='0';
          sstr[1]='\0';
        }


      /* Write the strings into the columns. */
      j=0;
      for(tmp=cols; tmp!=NULL; tmp=tmp->next)
        {
          switch(j)
            {
            case 0: ui16[i]=i;                                    break;
            case 1: gal_checkset_allocate_copy(extname, estra+i); break;
            case 2: gal_checkset_allocate_copy(tstr, tstra+i);    break;
            case 3: gal_checkset_allocate_copy(sstr, sstra+i);    break;
            }
          ++j;
        }


      /* Move to the next extension if we aren't on the last extension. */
      if( i!=numext-1 && fits_movrel_hdu(fptr, 1, &hdutype, &status) )
        {
          if( asprintf(&msg, "moving to hdu %zu", i+1)<0 )
            error(EXIT_FAILURE, 0, "%s: asprintf allocation", __func__);
          gal_fits_io_error(status, msg);
        }
    }

  /* Close the file. */
  fits_close_file(fptr, &status);


  /* Print the results. */
  if(!p->cp.quiet)
    {
      printf("%s\nRun on %s-----\n", PROGRAM_STRING, ctime(&p->rawtime));
      printf("HDU (extension) information: '%s'.\n", p->filename);
      printf(" Column 1: Index (counting from 0, usable with '--hdu').\n");
      printf(" Column 2: Name ('EXTNAME' in FITS standard, usable with "
             "'--hdu').\n");
      if(hasblankname)
        printf("           ('%s' means that no name is specified for this "
               "HDU)\n", GAL_BLANK_STRING);
      printf(" Column 3: Image data type or 'table' format (ASCII or "
             "binary).\n");
      printf(" Column 4: Size of data in HDU.\n");
      printf("-----\n");
    }
  gal_table_write(cols, NULL, NULL, GAL_TABLE_FORMAT_TXT, NULL, NULL, 0);
  gal_list_data_free(cols);
}





static void
fits_hdu_number(struct fitsparams *p)
{
  fitsfile *fptr;
  int numhdu, status=0;

  /* Read the first extension (necessary for reading the rest). */
  fptr=gal_fits_hdu_open(p->filename, "0", READONLY);

  /* Get the number of HDUs. */
  if( fits_get_num_hdus(fptr, &numhdu, &status) )
    gal_fits_io_error(status, "finding number of HDUs");

  /* Close the file. */
  fits_close_file(fptr, &status);

  /* Print the result. */
  printf("%d\n", numhdu);
}





static void
fits_datasum(struct fitsparams *p)
{
  int status=0;
  fitsfile *fptr;
  unsigned long datasum, hdusum;

  /* Read the desired extension (necessary for reading the rest). */
  fptr=gal_fits_hdu_open(p->filename, p->cp.hdu, READONLY);

  /* Calculate the checksum and datasum of the opened HDU. */
  fits_get_chksum(fptr, &datasum, &hdusum, &status);
  gal_fits_io_error(status, "estimating datasum");

  /* Close the file. */
  fits_close_file(fptr, &status);

  /* Print the datasum */
  printf("%ld\n", datasum);
}





static void
fits_pixelscale(struct fitsparams *p)
{
  int nwcs=0;
  size_t i, ndim=0;
  double *pixelscale;
  struct wcsprm *wcs;

  /* Read the desired WCS. */
  wcs=gal_wcs_read(p->filename, p->cp.hdu, 0, 0, &nwcs);

  /* If a WCS doesn't exist, let the user know and return. */
  if(wcs)
    ndim=wcs->naxis;
  else
    error(EXIT_FAILURE, 0, "%s (hdu %s): no WCS could be read by WCSLIB, "
          "hence the pixel-scale cannot be determined", p->filename,
          p->cp.hdu);

  /* Calculate the pixel-scale in each dimension. */
  pixelscale=gal_wcs_pixel_scale(wcs);

  /* If not in quiet-mode, print some extra information. We don't want the
     last number to have a space after it, so we'll write the last one
     outside the loop.*/
  if(p->cp.quiet==0)
    {
      printf("Basic information for --pixelscale (remove extra info "
             "with '--quiet' or '-q')\n");
      printf("  Input: %s (hdu %s) has %zu dimensions.\n", p->filename,
             p->cp.hdu, ndim);
      printf("  Pixel scale in each dimension:\n");
      for(i=0;i<ndim;++i)
        printf("    %zu: %g (%s/pixel)\n", i+1, pixelscale[i], wcs->cunit[i]);
    }
  else
    {
      for(i=0;i<ndim-1;++i)
        printf("%g ", pixelscale[i]);
      printf("%g\n", pixelscale[ndim-1]);
    }

  /* Clean up. */
  wcsfree(wcs);
  free(pixelscale);
}





static void
fits_skycoverage(struct fitsparams *p)
{
  fitsfile *fptr;
  struct wcsprm *wcs;
  int nwcs=0, type, status=0;
  char *name=NULL, *unit=NULL;
  gal_data_t *tmp, *coords=NULL;
  double *x, *y, *z, min[3], max[3];
  size_t i, ndim, numrows, *dsize=NULL;

  /* Read the desired WCS. */
  wcs=gal_wcs_read(p->filename, p->cp.hdu, 0, 0, &nwcs);

  /* If a WCS doesn't exist, let the user know and return. */
  if(wcs==NULL)
    error(EXIT_FAILURE, 0, "%s (hdu %s): no WCS could be read by WCSLIB, "
          "hence the sky coverage cannot be determined", p->filename,
          p->cp.hdu);

  /* Make sure the input HDU is an image. */
  if( gal_fits_hdu_format(p->filename, p->cp.hdu) != IMAGE_HDU )
    error(EXIT_FAILURE, 0, "%s (hdu %s): is not an image HDU, the "
          "'--skycoverage' option only applies to image extensions",
          p->filename, p->cp.hdu);

  /* Get the array information of the image. */
  fptr=gal_fits_hdu_open(p->filename, p->cp.hdu, READONLY);
  gal_fits_img_info(fptr, &type, &ndim, &dsize, &name, &unit);
  fits_close_file(fptr, &status);

  /* Abort if we have more than 3 dimensions. */
  if(ndim==1 || ndim>3)
    error(EXIT_FAILURE, 0, "%s (hdu: %s): has %zu dimensions. Currently "
          "'--skycoverage' only supports 2 or 3 dimensional datasets",
          p->filename, p->cp.hdu, ndim);

  /* Now that we have the number of dimensions in the image, allocate the
     space needed for the coordinates. */
  numrows = (ndim==2) ? 5 : 9;
  for(i=0;i<ndim;++i)
    {
      tmp=gal_data_alloc(NULL, GAL_TYPE_FLOAT64, 1, &numrows, NULL, 0,
                         p->cp.minmapsize, p->cp.quietmmap, NULL,
                         NULL, NULL);
      tmp->next=coords;
      coords=tmp;
    }

  /* Fill in the coordinate arrays, Note that 'dsize' is ordered in C
     dimensions, for the WCS conversion, we need to have the dimensions
     ordered in FITS/Fortran order. */
  switch(ndim)
    {
    case 2:
      x=coords->array;  y=coords->next->array;
      x[0] = 1;         y[0] = 1;
      x[1] = dsize[1];  y[1] = 1;
      x[2] = 1;         y[2] = dsize[0];
      x[3] = dsize[1];  y[3] = dsize[0];
      x[4] = dsize[1]/2 + (dsize[1]%2 ? 1 : 0.5f);
      y[4] = dsize[0]/2 + (dsize[0]%2 ? 1 : 0.5f);
      break;
    case 3:
      x=coords->array; y=coords->next->array; z=coords->next->next->array;
      x[0] = 1;        y[0] = 1;              z[0]=1;
      x[1] = dsize[2]; y[1] = 1;              z[1]=1;
      x[2] = 1;        y[2] = dsize[1];       z[2]=1;
      x[3] = dsize[2]; y[3] = dsize[1];       z[3]=1;
      x[4] = 1;        y[4] = 1;              z[4]=dsize[0];
      x[5] = dsize[2]; y[5] = 1;              z[5]=dsize[0];
      x[6] = 1;        y[6] = dsize[1];       z[6]=dsize[0];
      x[7] = dsize[2]; y[7] = dsize[1];       z[7]=dsize[0];
      x[8] = dsize[2]/2 + (dsize[2]%2 ? 1 : 0.5f);
      y[8] = dsize[1]/2 + (dsize[1]%2 ? 1 : 0.5f);
      z[8] = dsize[0]/2 + (dsize[0]%2 ? 1 : 0.5f);
      break;
    default:
      error(EXIT_FAILURE, 0, "%s: a bug! Please contact us at %s to "
            "fix the problem. 'ndim' of %zu is not recognized.",
            __func__, PACKAGE_BUGREPORT, ndim);
    }

  /* For a check:
  printf("IMAGE COORDINATES:\n");
  for(i=0;i<numrows;++i)
    if(ndim==2)
      printf("%-15g%-15g\n", x[i], y[i]);
    else
      printf("%-15g%-15g%-15g\n", x[i], y[i], z[i]);
  */

  /* Convert to the world coordinate system. */
  gal_wcs_img_to_world(coords, wcs, 1);

  /* For a check:
  printf("\nWORLD COORDINATES:\n");
  for(i=0;i<numrows;++i)
    if(ndim==2)
      printf("%-15g%-15g\n", x[i], y[i]);
    else
      printf("%-15g%-15g%-15g\n", x[i], y[i], z[i]);
  */

  /* Get the minimum and maximum values in each dimension. */
  tmp=gal_statistics_minimum(coords);
  min[0] = ((double *)(tmp->array))[0];      gal_data_free(tmp);
  tmp=gal_statistics_maximum(coords);
  max[0] = ((double *)(tmp->array))[0];      gal_data_free(tmp);
  tmp=gal_statistics_minimum(coords->next);
  min[1] = ((double *)(tmp->array))[0];      gal_data_free(tmp);
  tmp=gal_statistics_maximum(coords->next);
  max[1] = ((double *)(tmp->array))[0];      gal_data_free(tmp);
  if(ndim>2)
    {
      tmp=gal_statistics_minimum(coords->next->next);
      min[2] = ((double *)(tmp->array))[0];      gal_data_free(tmp);
      tmp=gal_statistics_maximum(coords->next->next);
      max[2] = ((double *)(tmp->array))[0];      gal_data_free(tmp);
    }

  /* Inform the user. */
  if(p->cp.quiet)
    {
      /* First print the center and full-width. */
      for(tmp=coords; tmp!=NULL; tmp=tmp->next)
        printf("%-15.10g ", ((double *)(tmp->array))[ndim==2 ? 4 : 8]);
      for(i=0;i<ndim;++i) printf("%-15.10g ", max[i]-min[i]);
      printf("\n");

      /* Then print the range in coordinates. */
      for(i=0;i<ndim;++i) printf("%-15.10g %-15.10g ", min[i], max[i]);
      printf("\n");
    }
  else
    {
      printf("Input file: %s (hdu: %s)\n", p->filename, p->cp.hdu);
      printf("\nSky coverage by center and (full) width:\n");
      switch(ndim)
        {
        case 2:
          printf("  Center: %-15.10g%-15.10g\n", x[4], y[4]);
          printf("  Width:  %-15.10g%-15.10g\n", max[0]-min[0], max[1]-min[1]);
          break;
        case 3:
          printf("  Center: %-15.10g%-15.10g%-15.10g\n", x[8], y[8], z[8]);
          printf("  width:  %-15.10g%-15.10g%-15.10g\n", max[0]-min[0],
                 max[1]-min[1], max[2]-min[2]);
          break;
        default:
          error(EXIT_FAILURE, 0, "%s: a bug! Please contact us at %s to fix "
                "the problem. 'ndim' value %zu is not recognized", __func__,
                PACKAGE_BUGREPORT, ndim);
        }

      /* For the range type of coverage. */
      printf("\nSky coverage by range along dimensions:\n");
      for(i=0;i<ndim;++i)
        printf("  %-8s %-15.10g%-15.10g\n", gal_wcs_dimension_name(wcs, i),
               min[i], max[i]);
    }

  /* Clean up. */
  free(dsize);
  wcsfree(wcs);
}





static void
fits_hdu_remove(struct fitsparams *p, int *r)
{
  char *hdu;
  fitsfile *fptr;
  int status=0, hdutype;

  while(p->remove)
    {
      /* Pop-out the top element. */
      hdu=gal_list_str_pop(&p->remove);

      /* Open the FITS file at the specified HDU. */
      fptr=gal_fits_hdu_open(p->filename, hdu, READWRITE);

      /* Delete the extension. */
      if( fits_delete_hdu(fptr, &hdutype, &status) )
        *r=fits_has_error(p, FITS_ACTION_REMOVE, hdu, status);
      status=0;

      /* Close the file. */
      fits_close_file(fptr, &status);
    }
}





/* This is similar to the library's 'gal_fits_open_to_write', except that
   it won't create an empty first extension.*/
fitsfile *
fits_open_to_write_no_blank(char *filename)
{
  int status=0;
  fitsfile *fptr;

  /* When the file exists just open it. Otherwise, create the file. But we
     want to leave the first extension as a blank extension and put the
     image in the next extension to be consistent between tables and
     images. */
  if(access(filename,F_OK) == -1 )
    {
      /* Create the file. */
      if( fits_create_file(&fptr, filename, &status) )
        gal_fits_io_error(status, NULL);
    }

  /* Open the file, ready for later steps. */
  if( fits_open_file(&fptr, filename, READWRITE, &status) )
    gal_fits_io_error(status, NULL);

  /* Return the pointer. */
  return fptr;
}





static void
fits_hdu_copy(struct fitsparams *p, int cut1_copy0, int *r)
{
  char *hdu;
  int status=0, hdutype;
  fitsfile *in, *out=NULL;
  gal_list_str_t *list = cut1_copy0 ? p->cut : p->copy;

  /* Copy all the given extensions. */
  while(list)
    {
      /* Pop-out the top element. */
      hdu=gal_list_str_pop(&list);

      /* Open the FITS file at the specified HDU. */
      in=gal_fits_hdu_open(p->filename, hdu,
                           cut1_copy0 ? READWRITE : READONLY);

      /* If the output isn't opened yet, open it.  */
      if(out==NULL)
        out = ( ( gal_fits_hdu_format(p->filename, hdu)==IMAGE_HDU
                  && p->primaryimghdu )
                ? fits_open_to_write_no_blank(p->cp.output)
                : gal_fits_open_to_write(p->cp.output) );


      /* Copy to the extension. */
      if( fits_copy_hdu(in, out, 0, &status) )
        *r=fits_has_error(p, FITS_ACTION_COPY, hdu, status);
      status=0;

      /* If this is a 'cut' operation, then remove the extension. */
      if(cut1_copy0)
        {
          if( fits_delete_hdu(in, &hdutype, &status) )
            *r=fits_has_error(p, FITS_ACTION_REMOVE, hdu, status);
          status=0;
        }

      /* Close the input file. */
      fits_close_file(in, &status);
    }

  /* Close the output file. */
  if(out) fits_close_file(out, &status);
}





int
fits(struct fitsparams *p)
{
  int r=EXIT_SUCCESS, printhduinfo=1;

  switch(p->mode)
    {
    /* Keywords, we have a separate set of functions in 'keywords.c'. */
    case FITS_MODE_KEY:
      r=keywords(p);
      break;

    /* HDU, functions defined here. */
    case FITS_MODE_HDU:

      /* Options that must be called alone. */
      if(p->numhdus) fits_hdu_number(p);
      else if(p->datasum) fits_datasum(p);
      else if(p->pixelscale) fits_pixelscale(p);
      else if(p->skycoverage) fits_skycoverage(p);

      /* Options that can be called together. */
      else
        {
          if(p->copy)
            {
              fits_hdu_copy(p, 0, &r);
              printhduinfo=0;
            }
          if(p->cut)
            {
              fits_hdu_copy(p, 1, &r);
              printhduinfo=0;
            }
          if(p->remove)
            {
              fits_hdu_remove(p, &r);
              printhduinfo=0;
            }

          if(printhduinfo)
            fits_print_extension_info(p);
        }
      break;

    /* Not recognized. */
    default:
      error(EXIT_FAILURE, 0, "%s: a bug! please contact us at %s to address "
            "the problem. The code %d is not recognized for p->mode",
            __func__, PACKAGE_BUGREPORT, p->mode);
    }

  return r;
}
