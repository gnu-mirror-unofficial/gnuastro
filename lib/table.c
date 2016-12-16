/*********************************************************************
txt -- Functions for I/O on text files.
This is part of GNU Astronomy Utilities (Gnuastro) package.

Original author:
     Mohammad Akhlaghi <akhlaghi@gnu.org>
Contributing author(s):
Copyright (C) 2016, Free Software Foundation, Inc.

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

#include <stdio.h>
#include <errno.h>
#include <error.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>

#include <gnuastro/txt.h>
#include <gnuastro/table.h>


/************************************************************************/
/***************         Information about a table        ***************/
/************************************************************************/
/* Store the information of each column in a table (either as a text file
   or as a FITS table) into an array of data structures with `numcols'
   structures (one data structure for each column). Note that the arrays in
   the data structures will be empty.

   The array of data structures will be returned, the number of columns
   will be put in the `numcols' argument and the type of the table (e.g.,
   ascii text file, or FITS binary or ASCII table) will be put in
   `tabletype' (macros defined in `gnuastro/table.h'. */
gal_data_t *
gal_table_info(char *filename, char *hdu, size_t *numcols, int *tabletype)
{
  /* Get the table type and size (number of columns and rows). */
  if(gal_fits_name_is_fits(filename))
    return gal_fits_table_info(filename, hdu, numcols, tabletype);
  else
    {
      *tabletype=GAL_TABLE_TYPE_TXT;
      return gal_txt_table_info(filename, numcols);
    }

  /* Abort with an error if we get to this point. */
  error(EXIT_FAILURE, 0, "A bug! please contact us at %s so we can fix "
        "the problem. For some reason, control has reached the end of "
        "`gal_table_info'", PACKAGE_BUGREPORT);
  return NULL;
}




















/************************************************************************/
/***************               Read a table               ***************/
/************************************************************************/
/* In programs, the `searchin' variable is much more easier to type in as a
   description than an integer (which is what `gal_table_read_cols'
   needs). This function will check the string value and give the
   corresponding integer value.*/
int
gal_table_searchin_from_str(char *searchin_str)
{
  if(strcmp(searchin_str, "name")==0)
    return GAL_TABLE_SEARCH_NAME;
  else if(strcmp(searchin_str, "unit")==0)
    return GAL_TABLE_SEARCH_UNIT;
  else if(strcmp(searchin_str, "comment")==0)
    return GAL_TABLE_SEARCH_COMMENT;
  else
    error(EXIT_FAILURE, 0, "`--searchin' only recognizes the values "
          "`name', `unit', and `comment', you have asked for `%s'",
          searchin_str);

  /* Report an error control reaches here. */
  error(EXIT_FAILURE, 0, "A bug! please contact us at %s so we can address "
        "the problem. For some reason control has reached the end of "
        "`gal_table_searchin_from_str'", PACKAGE_BUGREPORT);
  return -1;
}





/* Function to print regular expression error. This is taken from the GNU C
   library manual, with small modifications to fit out style, */
void
regexerrorexit(int errcode, regex_t *compiled, char *input)
{
  char *regexerrbuf;
  size_t length = regerror (errcode, compiled, NULL, 0);

  errno=0;
  regexerrbuf=malloc(length);
  if(regexerrbuf==NULL)
    error(EXIT_FAILURE, errno, "%zu bytes for regexerrbuf", length);
  (void) regerror(errcode, compiled, regexerrbuf, length);

  error(EXIT_FAILURE, 0, "Regular expression error: %s in value to "
        "`--column' (`-c'): `%s'", regexerrbuf, input);
}





/* Macro to set the string to search in */
#define SET_STRCHECK                                                    \
  strcheck=NULL;                                                        \
  switch(searchin)                                                      \
    {                                                                   \
    case GAL_TABLE_SEARCH_NAME:                                         \
      strcheck=allcols[i].name;                                         \
      break;                                                            \
    case GAL_TABLE_SEARCH_UNIT:                                         \
      strcheck=allcols[i].unit;                                         \
      break;                                                            \
    case GAL_TABLE_SEARCH_COMMENT:                                      \
      strcheck=allcols[i].comment;                                      \
      break;                                                            \
    default:                                                            \
      error(EXIT_FAILURE, 0, "the code %d to searchin was not "         \
            "recognized in gal_table_read_cols", searchin);             \
    }





static struct gal_linkedlist_sll *
make_list_of_indexs(struct gal_linkedlist_stll *cols, gal_data_t *allcols,
                    size_t numcols, int searchin, int ignorecase,
                    char *filename, char *hdu)
{
  size_t i;
  long tlong;
  int regreturn;
  regex_t *regex;
  char *str, *strcheck, *tailptr;
  struct gal_linkedlist_stll *tmp;
  struct gal_linkedlist_sll *indexll=NULL;

  for(tmp=cols; tmp!=NULL; tmp=tmp->next)
    {
      /* REGULAR EXPRESSION: When the first and last characters are `/'. */
      if( tmp->v[0]=='/' && tmp->v[strlen(tmp->v)-1]=='/' )
        {
          /* Remove the slashes, note that we don't want to change `tmp->v'
             (because it should be freed later). So first we set the last
             character to `\0', then define a new string from the first
             element. */
          tmp->v[strlen(tmp->v)-1]='\0';
          str = tmp->v + 1;

          /* Allocate the regex_t structure: */
          errno=0; regex=malloc(sizeof *regex);
          if(regex==NULL)
            error(EXIT_FAILURE, errno, "%zu bytes for regex", sizeof *regex);

          /* First we have to "compile" the string into the regular
             expression, see the "POSIX Regular Expression Compilation"
             section of the GNU C Library.

             About the case of the string: the FITS standard says: "It is
             _strongly recommended_ that every field of the table be
             assigned a unique, case insensitive name with this keyword..."
             So the column names can be case-sensitive.

             Here, we don't care about the details of a match, the only
             important thing is a match, so we are using the REG_NOSUB
             flag.*/
          regreturn=0;
          regreturn=regcomp(regex, str, ( ignorecase
                                          ? RE_SYNTAX_AWK | REG_ICASE
                                          : RE_SYNTAX_AWK ) );
          if(regreturn)
            regexerrorexit(regreturn, regex, str);


          /* With the regex structure "compile"d you can go through all the
             column names. Just note that column names are not mandatory in
             the FITS standard, so some (or all) columns might not have
             names, if so `p->tname[i]' will be NULL. */
          for(i=0;i<numcols;++i)
            {
              SET_STRCHECK;
              if(strcheck && regexec(regex, strcheck, 0, 0, 0)==0)
                gal_linkedlist_add_to_sll(&indexll, i);
            }

          /* Free the regex_t structure: */
          regfree(regex);
        }

      /* INTEGER: If the string is an integer, then tailptr should point to
         the null character. If it points to anything else, it shows that
         we are not dealing with an integer (usable as a column number). So
         floating point values are also not acceptable. */
      else if( (tlong=strtol(tmp->v, &tailptr, 0)) && *tailptr=='\0')
        {
          /* Make sure we are not dealing with a negative number! */
          if(tlong<0)
            error(EXIT_FAILURE, 0, "the column numbers given to the "
                  "must not be negative, you have asked for `%ld'", tlong);

          /* Check if the given value is not larger than the number of
             columns in the input catalog (note that the user is counting
             from 1, not 0!) */
          if(tlong>numcols)
            {
              if(gal_fits_name_is_fits(filename))
                error(EXIT_FAILURE, 0, "%s (hdu %s): has %zu columns, but "
                      "you have asked for column number %zu", filename,
                      hdu, numcols, tlong);
              else
                error(EXIT_FAILURE, 0, "%s: has %zu columns, but you have "
                      "asked for column number %zu", filename,
                      numcols, tlong);
            }

          /* Everything seems to be fine, put this column number in the
             output column numbers linked list. Note that internally, the
             column numbers start from 0, not 1.*/
          gal_linkedlist_add_to_sll(&indexll, tlong-1);
        }

      /* EXACT MATCH: */
      else
        {
          for(i=0;i<numcols;++i)
            {
              SET_STRCHECK;
              if(strcheck && strcmp(tmp->v, strcheck)==0)
                gal_linkedlist_add_to_sll(&indexll, i);
            }
        }
    }

  /* Reverse the list. */
  gal_linkedlist_reverse_sll(&indexll);
  return indexll;
}





/* Read the specified columns in a text file (named `filename') into a
   linked list of data structures. If the file is FITS, then `hdu' will
   also be used, otherwise, `hdu' is ignored. The information to search for
   columns should be specified by the `cols' linked list as string values
   in each node of the list. The `tosearch' value comes from the
   `gal_table_where_to_search' enumerator and has to be one of its given
   types. If `cols' is NULL, then this function will read the full table.

   The output is a linked list with the same order of the cols linked
   list. If all the columns are to be read, the output will be the in the
   order of the table, so the first column is the first popped data
   structure and so on.

   Recall that linked lists are last-in-first-out, so the last element you
   add to the list is the first one that this function will pop. If you
   want to reverse this order, you can call the
   `gal_linkedlist_reverse_stll' function in `linkedlist.h'.*/
gal_data_t *
gal_table_read_cols(char *filename, char *hdu,
                    struct gal_linkedlist_stll *cols, int searchin,
                    int ignorecase)
{
  int tabletype;
  size_t numcols;
  gal_data_t *allcols, *out=NULL;
  struct gal_linkedlist_sll *indexll;

  /* First get the information of all the columns. */
  allcols=gal_table_info(filename, hdu, &numcols, &tabletype);

  /* Get the list of indexs in the same order as the input list */
  indexll=make_list_of_indexs(cols, allcols, numcols, searchin,
                              ignorecase, filename, hdu);

  gal_linkedlist_print_sll(indexll);

  /* Reverse the list of indexs so the data structure linked list will be
     in the same order as the input list of columns.
  while(indexll!=NULL)
    {

    }
  */
  printf("\n\n-----\ntmp stop.\n\n");
  exit(0);
  return out;
}
