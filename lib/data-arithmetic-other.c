/*********************************************************************
Arithmetic operations on data structures.
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

#include <math.h>
#include <stdio.h>
#include <errno.h>
#include <error.h>
#include <stdlib.h>

#include <gnuastro/data.h>









/***********************************************************************/
/***************        Unary functions/operators         **************/
/***********************************************************************/
/* Change input data structure type. */
gal_data_t *
data_arithmetic_change_type(gal_data_t *data, int operator,
                            unsigned char flags)
{
  int type=0;
  gal_data_t *out;

  /* Set the output type. */
  switch(operator)
    {
    case GAL_DATA_OPERATOR_TO_UCHAR:    type=GAL_DATA_TYPE_UCHAR;    break;
    case GAL_DATA_OPERATOR_TO_CHAR:     type=GAL_DATA_TYPE_UCHAR;    break;
    case GAL_DATA_OPERATOR_TO_USHORT:   type=GAL_DATA_TYPE_USHORT;   break;
    case GAL_DATA_OPERATOR_TO_SHORT:    type=GAL_DATA_TYPE_SHORT;    break;
    case GAL_DATA_OPERATOR_TO_UINT:     type=GAL_DATA_TYPE_UINT;     break;
    case GAL_DATA_OPERATOR_TO_INT:      type=GAL_DATA_TYPE_INT;      break;
    case GAL_DATA_OPERATOR_TO_ULONG:    type=GAL_DATA_TYPE_ULONG;    break;
    case GAL_DATA_OPERATOR_TO_LONG:     type=GAL_DATA_TYPE_LONG;     break;
    case GAL_DATA_OPERATOR_TO_LONGLONG: type=GAL_DATA_TYPE_LONGLONG; break;
    case GAL_DATA_OPERATOR_TO_FLOAT:    type=GAL_DATA_TYPE_FLOAT;    break;
    case GAL_DATA_OPERATOR_TO_DOUBLE:   type=GAL_DATA_TYPE_DOUBLE;   break;

    default:
      error(EXIT_FAILURE, 0, "operator value of %d not recognized in "
            "`data_arithmetic_change_type'", operator);
    }

  /* Copy to the new type. */
  out=gal_data_copy_to_new_type(data, type);

  /* Delete the input structure if the user asked for it. */
  if(flags & GAL_DATA_ARITH_FREE)
    gal_data_free(data);

  /* Return */
  return out;
}





/* Return an array of value 1 for any zero valued element and zero for any
   non-zero valued element. */
#define TYPE_CASE_FOR_NOT(TYPE, IN, IN_FINISH) {                        \
    case TYPE:                                                          \
      do *o++ = !*IN; while(++IN<IN_FINISH);                            \
      break;                                                            \
  }

gal_data_t *
data_arithmetic_not(gal_data_t *data, unsigned char flags)
{
  gal_data_t *out;

  /* 'value' will only be read from one of these based on the
     datatype. Which the caller assigned. If there is any problem, it is
     their responsability, not this function's.*/
  unsigned char     *uc = data->array,   *ucf = data->array + data->size, *o;
  char               *c = data->array,    *cf = data->array + data->size;
  unsigned short    *us = data->array,   *usf = data->array + data->size;
  short              *s = data->array,    *sf = data->array + data->size;
  unsigned int      *ui = data->array,   *uif = data->array + data->size;
  int               *in = data->array,   *inf = data->array + data->size;
  unsigned long     *ul = data->array,   *ulf = data->array + data->size;
  long               *l = data->array,    *lf = data->array + data->size;
  LONGLONG           *L = data->array,    *Lf = data->array + data->size;
  float              *f = data->array,    *ff = data->array + data->size;
  double             *d = data->array,    *df = data->array + data->size;


  /* Allocate the output array. */
  out=gal_data_alloc(NULL, GAL_DATA_TYPE_UCHAR, data->ndim, data->dsize,
                     data->wcs, 0, data->minmapsize);
  o=out->array;


  /* Go over the pixels and set the output values. */
  switch(data->type)
    {

    TYPE_CASE_FOR_NOT(GAL_DATA_TYPE_UCHAR,    uc,  ucf)
    TYPE_CASE_FOR_NOT(GAL_DATA_TYPE_CHAR,     c,   cf)
    TYPE_CASE_FOR_NOT(GAL_DATA_TYPE_LOGICAL,  c,   cf)
    TYPE_CASE_FOR_NOT(GAL_DATA_TYPE_USHORT,   us,  usf)
    TYPE_CASE_FOR_NOT(GAL_DATA_TYPE_SHORT,    s,   sf)
    TYPE_CASE_FOR_NOT(GAL_DATA_TYPE_UINT,     ui,  uif)
    TYPE_CASE_FOR_NOT(GAL_DATA_TYPE_INT,      in,  inf)
    TYPE_CASE_FOR_NOT(GAL_DATA_TYPE_ULONG,    ul,  ulf)
    TYPE_CASE_FOR_NOT(GAL_DATA_TYPE_LONG,     l,   lf)
    TYPE_CASE_FOR_NOT(GAL_DATA_TYPE_LONGLONG, L,   Lf)
    TYPE_CASE_FOR_NOT(GAL_DATA_TYPE_FLOAT,    f,   ff)
    TYPE_CASE_FOR_NOT(GAL_DATA_TYPE_DOUBLE,   d,   df)

    case GAL_DATA_TYPE_BIT:
      error(EXIT_FAILURE, 0, "Currently Gnuastro doesn't support bit "
            "datatype, please get in touch with us to implement it.");

    default:
      error(EXIT_FAILURE, 0, "type value (%d) not recognized "
            "in `data_arithmetic_not'", data->type);
    }

  /* Delete the input structure if the user asked for it. */
  if(flags & GAL_DATA_ARITH_FREE)
    gal_data_free(data);

  /* Return */
  return out;
}
















/***********************************************************************/
/***************          Checking functions              **************/
/***********************************************************************/

/* Some functions are only for a floating point operand, so if the input
   isn't floating point, inform the user to change the type explicitly,
   doing it implicitly/internally puts too much responsability on the
   program. */
static void
check_float_input(gal_data_t *in, int operator, char *numstr)
{
  switch(in->type)
    {
    case GAL_DATA_TYPE_FLOAT:
    case GAL_DATA_TYPE_DOUBLE:
      break;
    default:
      error(EXIT_FAILURE, 0, "the %s operator can only accept single or "
            "double precision floating point numbers as its operand. The "
            "%s operand has type %s. You can use the `float' or `double' "
            "operators before this operator to explicity convert to the "
            "desired precision floating point type. If the operand was "
            "originally a typed number (string of characters), add an `f' "
            "after it so it is directly read into the proper precision "
            "floating point number (based on the number of non-zero "
            "decimals it has)", gal_data_operator_string(operator), numstr,
            gal_data_type_string(in->type));
    }
}




















/***********************************************************************/
/***************             Unary functions              **************/
/***********************************************************************/


#define UNIFUNC_RUN_FUNCTION(IT, OP){                                   \
    IT *ia=in->array, *oa=o->array, *of=oa + o->size;                   \
    do *oa++ = OP(*ia++); while(oa<of);                                 \
  }





#define UNIFUNC_F_OPERATOR_DONE(OP)                                     \
  switch(in->type)                                                      \
    {                                                                   \
    case GAL_DATA_TYPE_FLOAT:                                           \
      UNIFUNC_RUN_FUNCTION(float, OP);                                  \
      break;                                                            \
    case GAL_DATA_TYPE_DOUBLE:                                          \
      UNIFUNC_RUN_FUNCTION(double, OP);                                 \
      break;                                                            \
    default:                                                            \
      error(EXIT_FAILURE, 0, "type %d not recognized in "               \
            "for l->type in UNIFUNC_F_OPERATOR_DONE", in->type);        \
    }





gal_data_t *
data_arithmetic_unary_function_f(int operator, unsigned char flags,
                                 gal_data_t *in)
{
  gal_data_t *o;

  /* Check the input type. */
  check_float_input(in, operator, "first");

  /* If we want inplace output, set the output pointer to the input
     pointer, for every pixel, the operation will be independent. */
  if(flags & GAL_DATA_ARITH_INPLACE)
    o = in;
  else
    o = gal_data_alloc(NULL, in->type, in->ndim, in->dsize, in->wcs,
                       0, in->minmapsize);

  /* Start setting the operator and operands. */
  switch(operator)
    {
    case GAL_DATA_OPERATOR_SQRT:   UNIFUNC_F_OPERATOR_DONE( sqrt  ); break;
    case GAL_DATA_OPERATOR_LOG:    UNIFUNC_F_OPERATOR_DONE( log   ); break;
    case GAL_DATA_OPERATOR_LOG10:  UNIFUNC_F_OPERATOR_DONE( log10 ); break;
    default:
      error(EXIT_FAILURE, 0, "Operator code %d not recognized in "
            "data_arithmetic_binary_function", operator);
    }


  /* Clean up. Note that if the input arrays can be freed, and any of right
     or left arrays needed conversion, `UNIFUNC_CONVERT_TO_COMPILED_TYPE'
     has already freed the input arrays, and we only have `r' and `l'
     allocated in any case. Alternatively, when the inputs shouldn't be
     freed, the only allocated spaces are the `r' and `l' arrays if their
     types weren't compiled for binary operations, we can tell this from
     the pointers: if they are different from the original pointers, they
     were allocated. */
  if( (flags & GAL_DATA_ARITH_FREE) && o!=in)
    gal_data_free(in);

  /* Return */
  return o;
}




















/***********************************************************************/
/***************            Binary functions              **************/
/***********************************************************************/


#define BINFUNC_RUN_FUNCTION(OT, RT, LT, OP){                           \
    LT *la=l->array;                                                    \
    RT *ra=r->array;                                                    \
    OT *oa=o->array, *of=oa + o->size;                                  \
    if(l->size==r->size) do *oa = OP(*la++, *ra++); while(++oa<of);     \
    else if(l->size==1)  do *oa = OP(*la,   *ra++); while(++oa<of);     \
    else                 do *oa = OP(*la++, *ra  ); while(++oa<of);     \
  }





#define BINFUNC_F_OPERATOR_LEFT_RIGHT_SET(RT, LT, OP)                   \
  switch(o->type)                                                       \
    {                                                                   \
    case GAL_DATA_TYPE_FLOAT:                                           \
      BINFUNC_RUN_FUNCTION(float, RT, LT, OP);                          \
      break;                                                            \
    case GAL_DATA_TYPE_DOUBLE:                                          \
      BINFUNC_RUN_FUNCTION(double, RT, LT, OP);                         \
      break;                                                            \
    default:                                                            \
      error(EXIT_FAILURE, 0, "type %d not recognized in "               \
            "for o->type in BINFUNC_F_OPERATOR_LEFT_RIGHT_SET",         \
            o->type);                                                   \
    }





#define BINFUNC_F_OPERATOR_LEFT_SET(LT, OP)                             \
  switch(r->type)                                                       \
    {                                                                   \
    case GAL_DATA_TYPE_FLOAT:                                           \
      BINFUNC_F_OPERATOR_LEFT_RIGHT_SET(float, LT, OP);                 \
      break;                                                            \
    case GAL_DATA_TYPE_DOUBLE:                                          \
      BINFUNC_F_OPERATOR_LEFT_RIGHT_SET(double, LT, OP);                \
      break;                                                            \
    default:                                                            \
      error(EXIT_FAILURE, 0, "type %d not recognized in "               \
            "for r->type in BINFUNC_F_OPERATOR_LEFT_SET", r->type);    \
    }





#define BINFUNC_F_OPERATOR_SET(OP)                                      \
  switch(l->type)                                                       \
    {                                                                   \
    case GAL_DATA_TYPE_FLOAT:                                           \
      BINFUNC_F_OPERATOR_LEFT_SET(float, OP);                           \
      break;                                                            \
    case GAL_DATA_TYPE_DOUBLE:                                          \
      BINFUNC_F_OPERATOR_LEFT_SET(double, OP);                          \
      break;                                                            \
    default:                                                            \
      error(EXIT_FAILURE, 0, "type %d not recognized in "               \
            "for l->type in BINFUNC_F_OPERATOR_SET", l->type);          \
    }





gal_data_t *
data_arithmetic_binary_function_f(int operator, unsigned char flags,
                                  gal_data_t *l, gal_data_t *r)
{
  int final_otype;
  gal_data_t *o=NULL;
  size_t out_size, minmapsize;


  /* Simple sanity check on the input sizes */
  if( !( (flags & GAL_DATA_ARITH_NUMOK) && (l->size==1 || r->size==1))
      && gal_data_dsize_is_different(l, r) )
    error(EXIT_FAILURE, 0, "the input datasets don't have the same "
          "dimension/size in data_arithmetic_binary_function");

  /* Check for the types of the left and right operands. */
  check_float_input(l, operator, "first");
  check_float_input(r, operator, "second");

  /* Set the output type. */
  final_otype = gal_data_out_type(l, r);

  /* Set the output sizes. */
  minmapsize = ( l->minmapsize < r->minmapsize
                 ? l->minmapsize : r->minmapsize );
  out_size = l->size > r->size ? l->size : r->size;


  /* If we want inplace output, set the output pointer to one input. Note
     that the output type can be different from both inputs.  */
  if(flags & GAL_DATA_ARITH_INPLACE)
    {
      if     (l->type==final_otype && out_size==l->size)   o = l;
      else if(r->type==final_otype && out_size==r->size)   o = r;
    }


  /* If the output pointer was not set for any reason, allocate it. For
     `mmapsize', note that since its `size_t', it will always be
     Positive. The `-1' that is recommended to give when you want the value
     in RAM is actually the largest possible memory location. So we just
     have to choose the smaller minmapsize of the two to decide if the
     output array should be in RAM or not. */
  if(o==NULL)
    o = gal_data_alloc(NULL, final_otype,
                       l->size>1 ? l->ndim  : r->ndim,
                       l->size>1 ? l->dsize : r->dsize,
                       l->size>1 ? l->wcs : r->wcs, 0, minmapsize );


  /* Start setting the operator and operands. */
  switch(operator)
    {
    case GAL_DATA_OPERATOR_POW:  BINFUNC_F_OPERATOR_SET( pow  ); break;
    default:
      error(EXIT_FAILURE, 0, "Operator code %d not recognized in "
            "data_arithmetic_binary_function", operator);
    }


  /* Clean up. Note that if the input arrays can be freed, and any of right
     or left arrays needed conversion, `BINFUNC_CONVERT_TO_COMPILED_TYPE'
     has already freed the input arrays, and we only have `r' and `l'
     allocated in any case. Alternatively, when the inputs shouldn't be
     freed, the only allocated spaces are the `r' and `l' arrays if their
     types weren't compiled for binary operations, we can tell this from
     the pointers: if they are different from the original pointers, they
     were allocated. */
  if(flags & GAL_DATA_ARITH_FREE)
    {
      if     (o==l)       gal_data_free(r);
      else if(o==r)       gal_data_free(l);
      else              { gal_data_free(l); gal_data_free(r); }
    }

  /* Return */
  return o;
}




















/***********************************************************************/
/***************                  Where                   **************/
/***********************************************************************/
#define DO_WHERE_OPERATION(ITT, OT) {                                \
    ITT *it=iftrue->array;                                           \
    OT *o=out->array, *of=out->array+out->size;                      \
    if(iftrue->size==1)                                              \
      do   *o = *c++ ? *it : *o;         while(++o<of);              \
    else                                                             \
      do { *o = *c++ ? *it : *o; ++it; } while(++o<of);              \
}





#define WHERE_OUT_SET(OT)                                            \
  switch(iftrue->type)                                               \
    {                                                                \
    case GAL_DATA_TYPE_UCHAR:                                        \
      DO_WHERE_OPERATION(unsigned char, OT);                         \
      break;                                                         \
    case GAL_DATA_TYPE_CHAR:                                         \
      DO_WHERE_OPERATION(char, OT);                                  \
      break;                                                         \
    case GAL_DATA_TYPE_USHORT:                                       \
      DO_WHERE_OPERATION(unsigned short, OT);                        \
      break;                                                         \
    case GAL_DATA_TYPE_SHORT:                                        \
      DO_WHERE_OPERATION(short, OT);                                 \
      break;                                                         \
    case GAL_DATA_TYPE_UINT:                                         \
      DO_WHERE_OPERATION(unsigned int, OT);                          \
      break;                                                         \
    case GAL_DATA_TYPE_INT:                                          \
      DO_WHERE_OPERATION(int, OT);                                   \
      break;                                                         \
    case GAL_DATA_TYPE_ULONG:                                        \
      DO_WHERE_OPERATION(unsigned long, OT);                         \
      break;                                                         \
    case GAL_DATA_TYPE_LONG:                                         \
      DO_WHERE_OPERATION(long, OT);                                  \
      break;                                                         \
    case GAL_DATA_TYPE_LONGLONG:                                     \
      DO_WHERE_OPERATION(LONGLONG, OT);                              \
      break;                                                         \
    case GAL_DATA_TYPE_FLOAT:                                        \
      DO_WHERE_OPERATION(float, OT);                                 \
      break;                                                         \
    case GAL_DATA_TYPE_DOUBLE:                                       \
      DO_WHERE_OPERATION(double, OT);                                \
      break;                                                         \
    default:                                                         \
      error(EXIT_FAILURE, 0, "type code %d not recognized for the "  \
            "`iftrue' dataset of `WHERE_OUT_SET'", iftrue->type);    \
    }





void
data_arithmetic_where(int operator, unsigned char flags, gal_data_t *out,
                      gal_data_t *cond, gal_data_t *iftrue)
{
  unsigned char *c=cond->array;

  /* The condition operator has to be unsigned char. */
  if(cond->type!=GAL_DATA_TYPE_UCHAR)
    error(EXIT_FAILURE, 0, "the condition operand to "
          "`data_arithmetic_where' must be an `unsigned char' type, but "
          "the given condition operator has a `%s' type",
          gal_data_type_string(cond->type));

  /* The dimension and sizes of the out and condition data sets must be the
     same. */
  if(gal_data_dsize_is_different(out, cond))
    error(EXIT_FAILURE, 0, "the output and condition data sets of the "
          "`where' operator must be the same size");

  /* Do the operation. */
  switch(out->type)
    {
    case GAL_DATA_TYPE_UCHAR:
      WHERE_OUT_SET(unsigned char);
      break;
    case GAL_DATA_TYPE_CHAR:
      WHERE_OUT_SET(char);
      break;
    case GAL_DATA_TYPE_USHORT:
      WHERE_OUT_SET(unsigned short);
      break;
    case GAL_DATA_TYPE_SHORT:
      WHERE_OUT_SET(short);
      break;
    case GAL_DATA_TYPE_UINT:
      WHERE_OUT_SET(unsigned int);
      break;
    case GAL_DATA_TYPE_INT:
      WHERE_OUT_SET(int);
      break;
    case GAL_DATA_TYPE_ULONG:
      WHERE_OUT_SET(unsigned long);
      break;
    case GAL_DATA_TYPE_LONG:
      WHERE_OUT_SET(long);
      break;
    case GAL_DATA_TYPE_LONGLONG:
      WHERE_OUT_SET(LONGLONG);
      break;
    case GAL_DATA_TYPE_FLOAT:
      WHERE_OUT_SET(float);
      break;
    case GAL_DATA_TYPE_DOUBLE:
      WHERE_OUT_SET(double);
      break;
    default:
      error(EXIT_FAILURE, 0, "type code %d not recognized for the `out' "
            "dataset of `data_arithmetic_where'", out->type);
    }

  /* Clean up if necessary. */
  if(flags & GAL_DATA_ARITH_FREE)
    {
      gal_data_free(cond);
      gal_data_free(iftrue);
    }
}
