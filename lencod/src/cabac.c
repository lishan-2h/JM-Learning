/*
***********************************************************************
* COPYRIGHT AND WARRANTY INFORMATION
*
* Copyright 2001, International Telecommunications Union, Geneva
*
* DISCLAIMER OF WARRANTY
*
* These software programs are available to the user without any
* license fee or royalty on an "as is" basis. The ITU disclaims
* any and all warranties, whether express, implied, or
* statutory, including any implied warranties of merchantability
* or of fitness for a particular purpose.  In no event shall the
* contributor or the ITU be liable for any incidental, punitive, or
* consequential damages of any kind whatsoever arising from the
* use of these programs.
*
* This disclaimer of warranty extends to the user of these programs
* and user's customers, employees, agents, transferees, successors,
* and assigns.
*
* The ITU does not represent or warrant that the programs furnished
* hereunder are free of infringement of any third-party patents.
* Commercial implementations of ITU-T Recommendations, including
* shareware, may be subject to royalty fees to patent holders.
* Information regarding the ITU-T patent policy is available from
* the ITU Web site at http://www.itu.int.
*
* THIS IS NOT A GRANT OF PATENT RIGHTS - SEE THE ITU-T PATENT POLICY.
************************************************************************
*/

/*!
 *************************************************************************************
 * \file cabac.c
 *
 * \brief
 *    CABAC entropy coding routines
 *
 * \author
 *    Main contributors (see contributors.h for copyright, address and affiliation details)
 *    - Detlev Marpe                    <marpe@hhi.de>
 **************************************************************************************
 */

#include <stdlib.h>
#include <math.h>
#include <memory.h>
#include <assert.h>
#include "cabac.h"

/*!
 ************************************************************************
 * \brief
 *    Allocation of contexts models for the motion info
 *    used for arithmetic encoding
 ************************************************************************
 */
MotionInfoContexts* create_contexts_MotionInfo(void)
{
  MotionInfoContexts* enco_ctx;

  enco_ctx = (MotionInfoContexts*) calloc(1, sizeof(MotionInfoContexts) );
  if( enco_ctx == NULL )
    no_mem_exit("create_contexts_MotionInfo: enco_ctx");

  return enco_ctx;
}


/*!
 ************************************************************************
 * \brief
 *    Allocates of contexts models for the texture info
 *    used for arithmetic encoding
 ************************************************************************
 */
TextureInfoContexts* create_contexts_TextureInfo(void)
{
  TextureInfoContexts*  enco_ctx;

  enco_ctx = (TextureInfoContexts*) calloc(1, sizeof(TextureInfoContexts) );
  if( enco_ctx == NULL )
    no_mem_exit("create_contexts_TextureInfo: enco_ctx");

  return enco_ctx;
}


/*!
 ************************************************************************
 * \brief
 *    Initializes an array of contexts models with some pre-defined
 *    counts (ini_flag = 1) or with a flat histogram (ini_flag = 0)
 ************************************************************************
 */
#define INIT_CTX(jj,ii,ctx,ini) {for(j=0;j<jj;j++)for(i=0;i<ii;i++)biari_init_context((ctx)[j]+i,(ini)[j][i]);}


void init_contexts_MotionInfo (MotionInfoContexts *enco_ctx)
{
  int i,j;

  INIT_CTX (2, NUM_ABT_MODE_CTX, enco_ctx->ABT_mode_contexts, ABT_MODE_Ini);
  INIT_CTX (3, NUM_MB_TYPE_CTX,  enco_ctx->mb_type_contexts,  MB_TYPE_Ini );
  INIT_CTX (2, NUM_B8_TYPE_CTX,  enco_ctx->b8_type_contexts,  B8_TYPE_Ini );
  INIT_CTX (2, NUM_MV_RES_CTX,   enco_ctx->mv_res_contexts,   MV_RES_Ini  );
  INIT_CTX (2, NUM_REF_NO_CTX,   enco_ctx->ref_no_contexts,   REF_NO_Ini  );
  INIT_CTX (2, NUM_REF_NO_CTX,   enco_ctx->bwd_ref_no_contexts,   REF_NO_Ini  );
    
  INIT_CTX (1, NUM_DELTA_QP_CTX, &enco_ctx->delta_qp_contexts,	&DELTA_QP_Ini  );
  INIT_CTX (1, NUM_MB_AFF_CTX,	 &enco_ctx->mb_aff_contexts,		&MB_AFF_Ini  );

  enco_ctx->slice_term_context.state = 63;
  enco_ctx->slice_term_context.MPS   =  0;
}

/*!
 ************************************************************************
 * \brief
 *    Initializes an array of contexts models with some pre-defined
 *    counts (ini_flag = 1) or with a flat histogram (ini_flag = 0)
 ************************************************************************
 */
void init_contexts_TextureInfo(TextureInfoContexts *enco_ctx)
{
  int i,j;
  int intra = (img->type==INTRA_IMG ? 1 : 0);

  INIT_CTX (3, NUM_CBP_CTX,  enco_ctx->cbp_contexts,    CBP_Ini[!intra]);
  INIT_CTX (9, NUM_IPR_CTX,  enco_ctx->ipr_contexts,    IPR_Ini   );
  INIT_CTX (1, NUM_CIPR_CTX, &enco_ctx->cipr_contexts,   &CIPR_Ini  ); //GB

  INIT_CTX (NUM_BLOCK_TYPES, NUM_BCBP_CTX,  enco_ctx->bcbp_contexts, BCBP_Ini[intra]);
  INIT_CTX (NUM_BLOCK_TYPES, NUM_MAP_CTX,   enco_ctx->map_contexts,  MAP_Ini [intra]);
  INIT_CTX (NUM_BLOCK_TYPES, NUM_LAST_CTX,  enco_ctx->last_contexts, LAST_Ini[intra]);
  INIT_CTX (NUM_BLOCK_TYPES, NUM_ONE_CTX,   enco_ctx->one_contexts,  ONE_Ini [intra]);
  INIT_CTX (NUM_BLOCK_TYPES, NUM_ABS_CTX,   enco_ctx->abs_contexts,  ABS_Ini [intra]);
}


/*!
 ************************************************************************
 * \brief
 *    Frees the memory of the contexts models
 *    used for arithmetic encoding of the motion info.
 ************************************************************************
 */
void delete_contexts_MotionInfo(MotionInfoContexts *enco_ctx)
{
  if( enco_ctx == NULL )
    return;

  free( enco_ctx );

  return;
}

/*!
 ************************************************************************
 * \brief
 *    Frees the memory of the contexts models
 *    used for arithmetic encoding of the texture info.
 ************************************************************************
 */
void delete_contexts_TextureInfo(TextureInfoContexts *enco_ctx)
{
  if( enco_ctx == NULL )
    return;

  free( enco_ctx );

  return;
}


/*!
 **************************************************************************
 * \brief
 *    generates arithmetic code and passes the code to the buffer
 **************************************************************************
 */
int writeSyntaxElement_CABAC(SyntaxElement *se, DataPartition *this_dataPart)
{
  int curr_len;
  EncodingEnvironmentPtr eep_dp = &(this_dataPart->ee_cabac);

  curr_len = arienco_bits_written(eep_dp);

  // perform the actual coding by calling the appropriate method
  se->writing(se, eep_dp);

  if(se->type != SE_HEADER)
    this_dataPart->bitstream->write_flag = 1;

  return (se->len = (arienco_bits_written(eep_dp) - curr_len));
}

/*!
 ***************************************************************************
 * \brief
 *    This function is used to arithmetically encode the field
 *    mode info of a given MB  in the case of mb-based frame/field decision
 ***************************************************************************
 */
void writeFieldModeInfo2Buffer_CABAC(SyntaxElement *se, EncodingEnvironmentPtr eep_dp)
{
	//biari_encode_symbol_eq_prob (eep_dp, (signed short) se->value1);

	int a,b,act_ctx;
  MotionInfoContexts *ctx         = (img->currentSlice)->mot_ctx;
  Macroblock         *currMB      = &img->mb_data[img->current_mb_nr];
  int                mb_field = se->value1;
  
	if (currMB->field_available[0] == NULL)
		b = 0;
  else
    b = currMB->field_available[0]->mb_field;
  if (currMB->field_available[1] == NULL)
    a = 0;
  else
    a = currMB->field_available[1]->mb_field;

  act_ctx = a + b;

	if (mb_field==0) // frame
		biari_encode_symbol(eep_dp, 0,&ctx->mb_aff_contexts[act_ctx]);
  else
		biari_encode_symbol(eep_dp, 1,&ctx->mb_aff_contexts[act_ctx]);

	se->context = act_ctx;
//	if (img->currentSlice->write_is_real == 1)
//		img->currentSlice->field_ctx[act_ctx][mb_field]++;
	return;
}

/*!
 ***************************************************************************
 * \brief
 *    This function is used to arithmetically encode the mb_skip_flag.
 ***************************************************************************
 */
void writeMB_skip_flagInfo2Buffer_CABAC(SyntaxElement *se, EncodingEnvironmentPtr eep_dp)
{
	int a,b,act_ctx;
	int bframe   = (img->type==B_IMG || img->type==BS_IMG);
  MotionInfoContexts *ctx         = (img->currentSlice)->mot_ctx;
  Macroblock         *currMB      = &img->mb_data[img->current_mb_nr];
  int                curr_mb_type = se->value1;

	if (bframe)
  {
    if (currMB->skip_mb_available[0][1] == NULL)
      b = 0;
    else
      b = (currMB->skip_mb_available[0][1]->mb_type==0 && currMB->skip_mb_available[0][1]->cbp==0 ? 0 : 1);
		if (currMB->skip_mb_available[1][0] == NULL)
      a = 0;
    else
      a = (currMB->skip_mb_available[1][0]->mb_type==0 && currMB->skip_mb_available[1][0]->cbp==0 ? 0 : 1);
    act_ctx = 7 + a + b;

    if (se->value1==0 && se->value2==0) // DIRECT mode, no coefficients
			biari_encode_symbol (eep_dp, 0, &ctx->mb_type_contexts[2][act_ctx]);
    else
			biari_encode_symbol (eep_dp, 1, &ctx->mb_type_contexts[2][act_ctx]);
     
  }
	else
	{
    if (currMB->skip_mb_available[0][1] == NULL)
      b = 0;
    else
      b = (( (currMB->skip_mb_available[0][1])->mb_type != 0) ? 1 : 0 );
    if (currMB->skip_mb_available[1][0] == NULL)
      a = 0;
    else
      a = (( (currMB->skip_mb_available[1][0])->mb_type != 0) ? 1 : 0 );

    act_ctx = a + b;

		if (curr_mb_type==0) // SKIP
			biari_encode_symbol(eep_dp, 0,&ctx->mb_type_contexts[1][act_ctx]);
    else
			biari_encode_symbol(eep_dp, 1,&ctx->mb_type_contexts[1][act_ctx]);
	}
	se->context = act_ctx;
	return;
}

/*!
 ***************************************************************************
 * \brief
 *    This function is used to arithmetically encode the macroblock
 *    type info of a given MB.
 ***************************************************************************
 */

void writeMB_typeInfo2Buffer_CABAC(SyntaxElement *se, EncodingEnvironmentPtr eep_dp)
{
  int a, b;
  int act_ctx;
  int act_sym;
  int csym;
  int bframe   = (img->type==B_IMG || img->type==BS_IMG);
  int mode_sym = 0;
  int mode16x16;
  int useABT   = ((input->abt==INTER_INTRA_ABT) || (input->abt==INTER_ABT));


  MotionInfoContexts *ctx         = (img->currentSlice)->mot_ctx;
  Macroblock         *currMB      = &img->mb_data[img->current_mb_nr];
  int                curr_mb_type = se->value1;

  if(img->type == INTRA_IMG)  // INTRA-frame
  {
    if (currMB->mb_available[0][1] == NULL)
      b = 0;
    else 
      b = (( (currMB->mb_available[0][1])->mb_type != I4MB) ? 1 : 0 );
    if (currMB->mb_available[1][0] == NULL)
      a = 0;
    else 
      a = (( (currMB->mb_available[1][0])->mb_type != I4MB) ? 1 : 0 );

    act_ctx     = a + b;
    act_sym     = curr_mb_type;
    se->context = act_ctx; // store context

    if (act_sym==0) // 4x4 Intra
    {
      biari_encode_symbol(eep_dp, 0, ctx->mb_type_contexts[0] + act_ctx );
    }
    else // 16x16 Intra
    {
      biari_encode_symbol(eep_dp, 1, ctx->mb_type_contexts[0] + act_ctx );
      mode_sym = act_sym-1; // Values in the range of 0...23
      act_ctx  = 4;
      act_sym  = mode_sym/12;
      biari_encode_symbol(eep_dp, (unsigned char) act_sym, ctx->mb_type_contexts[0] + act_ctx ); // coding of AC/no AC
      mode_sym = mode_sym % 12;
      act_sym  = mode_sym / 4; // coding of cbp: 0,1,2
      act_ctx  = 5;
      if (act_sym==0)
      {
        biari_encode_symbol(eep_dp, 0, ctx->mb_type_contexts[0] + act_ctx );
      }
      else
      {
        biari_encode_symbol(eep_dp, 1, ctx->mb_type_contexts[0] + act_ctx );
        act_ctx=6;
        if (act_sym==1)
        {
          biari_encode_symbol(eep_dp, 0, ctx->mb_type_contexts[0] + act_ctx );
        }
        else
        {
          biari_encode_symbol(eep_dp, 1, ctx->mb_type_contexts[0] + act_ctx );
        }
      }
      mode_sym = mode_sym % 4; // coding of I pred-mode: 0,1,2,3
      act_sym  = mode_sym/2;
      act_ctx  = 7;
      biari_encode_symbol(eep_dp, (unsigned char) act_sym, ctx->mb_type_contexts[0] + act_ctx );
      act_ctx  = 8;
      act_sym  = mode_sym%2;
      biari_encode_symbol(eep_dp, (unsigned char) act_sym, ctx->mb_type_contexts[0] + act_ctx );
    }
  }
  else // INTER
  {

		if (bframe)
		{
			if (currMB->mb_available[0][1] == NULL)
				b = 0;
			else
				b = (( (currMB->mb_available[0][1])->mb_type != 0) ? 1 : 0 );
			if (currMB->mb_available[1][0] == NULL)
				a = 0;
			else
				a = (( (currMB->mb_available[1][0])->mb_type != 0) ? 1 : 0 );
			act_ctx = a + b;
			se->context = act_ctx; // store context
		}
    act_sym = curr_mb_type;

    if (act_sym>=(mode16x16=(bframe?24:7)))
    {
      mode_sym = act_sym-mode16x16;
      act_sym  = mode16x16; // 16x16 mode info
    }



    if (!bframe)
    {
      switch (act_sym)
      {
      case 0:
        break;
      case 1:
        biari_encode_symbol (eep_dp, 0, &ctx->mb_type_contexts[1][4]);
        biari_encode_symbol (eep_dp, 0, &ctx->mb_type_contexts[1][5]);
        biari_encode_symbol (eep_dp, 0, &ctx->mb_type_contexts[1][6]);
        break;
      case 2:
        biari_encode_symbol (eep_dp, 0, &ctx->mb_type_contexts[1][4]);
        biari_encode_symbol (eep_dp, 1, &ctx->mb_type_contexts[1][5]);
        biari_encode_symbol (eep_dp, 1, &ctx->mb_type_contexts[1][7]);
        break;
      case 3:
        biari_encode_symbol (eep_dp, 0, &ctx->mb_type_contexts[1][4]);
        biari_encode_symbol (eep_dp, 1, &ctx->mb_type_contexts[1][5]);
        biari_encode_symbol (eep_dp, 0, &ctx->mb_type_contexts[1][7]);
        break;
      case 4:
      case 5:
        biari_encode_symbol (eep_dp, 0, &ctx->mb_type_contexts[1][4]);
        biari_encode_symbol (eep_dp, 0, &ctx->mb_type_contexts[1][5]);
        biari_encode_symbol (eep_dp, 1, &ctx->mb_type_contexts[1][6]);
        break;
      case 6:
        biari_encode_symbol (eep_dp, 1, &ctx->mb_type_contexts[1][4]);
        if (!useABT)
          biari_encode_symbol (eep_dp, 0, &ctx->mb_type_contexts[1][7]);
        break;
      case 7:
        biari_encode_symbol (eep_dp, 1, &ctx->mb_type_contexts[1][4]);
        biari_encode_symbol (eep_dp, 1, &ctx->mb_type_contexts[1][7]);
        break;
      default:
        printf ("Unsupported MB-MODE in writeMB_typeInfo2Buffer_CABAC!\n");
        exit (1);
      }
    }
    else //===== B-FRAMES =====
    {
      if (act_sym==0)
      {
        biari_encode_symbol (eep_dp, 0, &ctx->mb_type_contexts[2][act_ctx]);
      }
      else if (act_sym<=2)
      {
        biari_encode_symbol (eep_dp, 1, &ctx->mb_type_contexts[2][act_ctx]);
        biari_encode_symbol (eep_dp, 0, &ctx->mb_type_contexts[2][4]);
        csym=act_sym-1;
        if (csym) biari_encode_symbol (eep_dp, 1, &ctx->mb_type_contexts[2][6]);
        else      biari_encode_symbol (eep_dp, 0, &ctx->mb_type_contexts[2][6]);
      }
      else if (act_sym<=10)
      {
        biari_encode_symbol (eep_dp, 1, &ctx->mb_type_contexts[2][act_ctx]);
        biari_encode_symbol (eep_dp, 1, &ctx->mb_type_contexts[2][4]);
        biari_encode_symbol (eep_dp, 0, &ctx->mb_type_contexts[2][5]);
        csym=(((act_sym-3)>>2)&0x01);
        if (csym) biari_encode_symbol (eep_dp, 1, &ctx->mb_type_contexts[2][6]);
        else      biari_encode_symbol (eep_dp, 0, &ctx->mb_type_contexts[2][6]);
        csym=(((act_sym-3)>>1)&0x01);
        if (csym) biari_encode_symbol (eep_dp, 1, &ctx->mb_type_contexts[2][6]);
        else      biari_encode_symbol (eep_dp, 0, &ctx->mb_type_contexts[2][6]);
        csym=((act_sym-3)&0x01);
        if (csym) biari_encode_symbol (eep_dp, 1, &ctx->mb_type_contexts[2][6]);
        else      biari_encode_symbol (eep_dp, 0, &ctx->mb_type_contexts[2][6]);
      }
      else if (act_sym==11 || act_sym==22)
      {
        biari_encode_symbol (eep_dp, 1, &ctx->mb_type_contexts[2][act_ctx]);
        biari_encode_symbol (eep_dp, 1, &ctx->mb_type_contexts[2][4]);
        biari_encode_symbol (eep_dp, 1, &ctx->mb_type_contexts[2][5]);
        biari_encode_symbol (eep_dp, 1, &ctx->mb_type_contexts[2][6]);
        biari_encode_symbol (eep_dp, 1, &ctx->mb_type_contexts[2][6]);
        if (act_sym==11)  biari_encode_symbol (eep_dp, 0, &ctx->mb_type_contexts[2][6]);
        else              biari_encode_symbol (eep_dp, 1, &ctx->mb_type_contexts[2][6]);
      }
      else
      {
        if (act_sym > 22) act_sym--;
        biari_encode_symbol (eep_dp, 1, &ctx->mb_type_contexts[2][act_ctx]);
        biari_encode_symbol (eep_dp, 1, &ctx->mb_type_contexts[2][4]);
        biari_encode_symbol (eep_dp, 1, &ctx->mb_type_contexts[2][5]);
        csym=(((act_sym-12)>>3)&0x01);
        if (csym) biari_encode_symbol (eep_dp, 1, &ctx->mb_type_contexts[2][6]);
        else      biari_encode_symbol (eep_dp, 0, &ctx->mb_type_contexts[2][6]);
        csym=(((act_sym-12)>>2)&0x01);
        if (csym) biari_encode_symbol (eep_dp, 1, &ctx->mb_type_contexts[2][6]);
        else      biari_encode_symbol (eep_dp, 0, &ctx->mb_type_contexts[2][6]);
        csym=(((act_sym-12)>>1)&0x01);
        if (csym) biari_encode_symbol (eep_dp, 1, &ctx->mb_type_contexts[2][6]);
        else      biari_encode_symbol (eep_dp, 0, &ctx->mb_type_contexts[2][6]);
        if ((!useABT) || (act_sym != 22))
        {
          csym=((act_sym-12)&0x01);
          if (csym) biari_encode_symbol (eep_dp, 1, &ctx->mb_type_contexts[2][6]); // GB not use if abt
          else      biari_encode_symbol (eep_dp, 0, &ctx->mb_type_contexts[2][6]);
        }
        if (act_sym >=22) act_sym++;
      }
    }

    if(act_sym==mode16x16) // additional info for 16x16 Intra-mode
    {
      act_ctx = 8;
      act_sym = mode_sym/12;
      biari_encode_symbol(eep_dp, (unsigned char) act_sym, ctx->mb_type_contexts[1] + act_ctx ); // coding of AC/no AC
      mode_sym = mode_sym % 12;

      act_sym = mode_sym / 4; // coding of cbp: 0,1,2
      act_ctx = 9;
      if (act_sym==0)
      {
        biari_encode_symbol(eep_dp, 0, ctx->mb_type_contexts[1] + act_ctx );
      }
      else
      {
        biari_encode_symbol(eep_dp, 1, ctx->mb_type_contexts[1] + act_ctx );
        if (act_sym==1)
        {
          biari_encode_symbol(eep_dp, 0, ctx->mb_type_contexts[1] + act_ctx );
        }
        else
        {
          biari_encode_symbol(eep_dp, 1, ctx->mb_type_contexts[1] + act_ctx );
        }
      }

      mode_sym = mode_sym % 4; // coding of I pred-mode: 0,1,2,3
      act_ctx  = 10;
      act_sym  = mode_sym/2;
      biari_encode_symbol(eep_dp, (unsigned char) act_sym, ctx->mb_type_contexts[1] + act_ctx );
      act_sym  = mode_sym%2;
      biari_encode_symbol(eep_dp, (unsigned char) act_sym, ctx->mb_type_contexts[1] + act_ctx );
    }
  }
}

/*!
 ***************************************************************************
 * \brief
 *    This function is used to arithmetically encode the intra_block_modeABT
 ***************************************************************************
 */
void writeABTIntraBlkModeInfo2Buffer_CABAC(SyntaxElement *se, EncodingEnvironmentPtr eep_dp)
{
  int                 ftype  = (img->type==INTRA_IMG?0:1);
  MotionInfoContexts *ctx    = (img->currentSlice)->mot_ctx;

  switch (se->value1)
  {
  case 0: //4x4 block mode: bins 10 
    biari_encode_symbol (eep_dp, 1, ctx->ABT_mode_contexts[ftype]+0);
    biari_encode_symbol (eep_dp, 0, ctx->ABT_mode_contexts[ftype]+2);
    break;
  case 1: //4x8 block mode: bins 00 
    biari_encode_symbol (eep_dp, 0, ctx->ABT_mode_contexts[ftype]+0);
    biari_encode_symbol (eep_dp, 0, ctx->ABT_mode_contexts[ftype]+1);
    break;
  case 2: //8x4 block mode: bins 01
    biari_encode_symbol (eep_dp, 0, ctx->ABT_mode_contexts[ftype]+0);
    biari_encode_symbol (eep_dp, 1, ctx->ABT_mode_contexts[ftype]+1);
    break;
  case 3: //8x8 block mode: bins 11
    biari_encode_symbol (eep_dp, 1, ctx->ABT_mode_contexts[ftype]+0);
    biari_encode_symbol (eep_dp, 1, ctx->ABT_mode_contexts[ftype]+2);
    break;
  }
}


/*!
 ***************************************************************************
 * \brief
 *    This function is used to arithmetically encode the 8x8 block
 *    type info
 ***************************************************************************
 */
void writeB8_typeInfo2Buffer_CABAC(SyntaxElement *se, EncodingEnvironmentPtr eep_dp)
{
  int act_ctx;
  int act_sym, csym;
  int bframe=(img->type==B_IMG || img->type==BS_IMG);

  MotionInfoContexts *ctx    = (img->currentSlice)->mot_ctx;

  act_sym = se->value1;
  act_ctx = 0;

  if (!bframe)
  {
    switch (act_sym)
    {
    case 0:
      biari_encode_symbol (eep_dp, 1, &ctx->b8_type_contexts[0][1]);
      break;
    case 1:
      biari_encode_symbol (eep_dp, 0, &ctx->b8_type_contexts[0][1]);
      biari_encode_symbol (eep_dp, 0, &ctx->b8_type_contexts[0][2]);
      biari_encode_symbol (eep_dp, 0, &ctx->b8_type_contexts[0][3]);
      break;
    case 2:
      biari_encode_symbol (eep_dp, 0, &ctx->b8_type_contexts[0][1]);
      biari_encode_symbol (eep_dp, 0, &ctx->b8_type_contexts[0][2]);
      biari_encode_symbol (eep_dp, 1, &ctx->b8_type_contexts[0][3]);
      biari_encode_symbol (eep_dp, 1, &ctx->b8_type_contexts[0][4]);
      break;
    case 3:
      biari_encode_symbol (eep_dp, 0, &ctx->b8_type_contexts[0][1]);
      biari_encode_symbol (eep_dp, 0, &ctx->b8_type_contexts[0][2]);
      biari_encode_symbol (eep_dp, 1, &ctx->b8_type_contexts[0][3]);
      biari_encode_symbol (eep_dp, 0, &ctx->b8_type_contexts[0][4]);
      break;
    case 4:
      biari_encode_symbol (eep_dp, 0, &ctx->b8_type_contexts[0][1]);
      biari_encode_symbol (eep_dp, 1, &ctx->b8_type_contexts[0][2]);
      break;
    }
  }
  else //===== B-FRAME =====
  {
    if (act_sym==0)
    {
      biari_encode_symbol (eep_dp, 0, &ctx->b8_type_contexts[1][0]);
      return;
    }
    else
    {
      biari_encode_symbol (eep_dp, 1, &ctx->b8_type_contexts[1][0]);
      act_sym--;
    }
    if (act_sym<2)
    {
      biari_encode_symbol (eep_dp, 0, &ctx->b8_type_contexts[1][1]);
      if (act_sym==0)   biari_encode_symbol (eep_dp, 0, &ctx->b8_type_contexts[1][3]);
      else              biari_encode_symbol (eep_dp, 1, &ctx->b8_type_contexts[1][3]);
    }
    else if (act_sym<6)
    {
      biari_encode_symbol (eep_dp, 1, &ctx->b8_type_contexts[1][1]);
      biari_encode_symbol (eep_dp, 0, &ctx->b8_type_contexts[1][2]);
      csym=(((act_sym-2)>>1)&0x01);
      if (csym) biari_encode_symbol (eep_dp, 1, &ctx->b8_type_contexts[1][3]);
      else      biari_encode_symbol (eep_dp, 0, &ctx->b8_type_contexts[1][3]);
      csym=((act_sym-2)&0x01);
      if (csym) biari_encode_symbol (eep_dp, 1, &ctx->b8_type_contexts[1][3]);
      else      biari_encode_symbol (eep_dp, 0, &ctx->b8_type_contexts[1][3]);
    }
    else if (act_sym==12)
    {
      biari_encode_symbol (eep_dp, 1, &ctx->b8_type_contexts[1][1]);
      biari_encode_symbol (eep_dp, 1, &ctx->b8_type_contexts[1][2]);
      biari_encode_symbol (eep_dp, 1, &ctx->b8_type_contexts[1][3]);
      biari_encode_symbol (eep_dp, 1, &ctx->b8_type_contexts[1][3]);
    }
    else
    {
      biari_encode_symbol (eep_dp, 1, &ctx->b8_type_contexts[1][1]);
      biari_encode_symbol (eep_dp, 1, &ctx->b8_type_contexts[1][2]);
      csym=(((act_sym-6)>>2)&0x01);
      if (csym) biari_encode_symbol (eep_dp, 1, &ctx->b8_type_contexts[1][3]);
      else      biari_encode_symbol (eep_dp, 0, &ctx->b8_type_contexts[1][3]);
      csym=(((act_sym-6)>>1)&0x01);
      if (csym) biari_encode_symbol (eep_dp, 1, &ctx->b8_type_contexts[1][3]);
      else      biari_encode_symbol (eep_dp, 0, &ctx->b8_type_contexts[1][3]);
      csym=((act_sym-6)&0x01);
      if (csym) biari_encode_symbol (eep_dp, 1, &ctx->b8_type_contexts[1][3]);
      else      biari_encode_symbol (eep_dp, 0, &ctx->b8_type_contexts[1][3]);
    }
  }
}


/*!
 ****************************************************************************
 * \brief
 *    This function is used to arithmetically encode a pair of
 *    intra prediction modes of a given MB.
 ****************************************************************************
 */
void writeIntraPredMode2Buffer_CABAC(SyntaxElement *se, EncodingEnvironmentPtr eep_dp)
{
  TextureInfoContexts *ctx     = img->currentSlice->tex_ctx;

  unary_bin_max_encode(eep_dp,(unsigned int) se->value1+1,ctx->ipr_contexts[0],1,8);
}


/*!
 ****************************************************************************
 * \brief
 *    This function is used to arithmetically encode the reference
 *    parameter of a given MB.
 ****************************************************************************
 */
void writeRefFrame2Buffer_CABAC(SyntaxElement *se, EncodingEnvironmentPtr eep_dp)
{
  MotionInfoContexts  *ctx    = img->currentSlice->mot_ctx;
  Macroblock          *currMB = &img->mb_data[img->current_mb_nr];
  int                 addctx  = se->context;

  int   a, b;
  int   act_ctx;
  int   act_sym;
  int** refframe_array = ((img->type==B_IMG || img->type==BS_IMG)? fw_refFrArr : refFrArr);
  int   block_y        = img->block_y;

  if( input->InterlaceCodingOption >= MB_CODING && mb_adaptive )
  {
    if( !img->field_mode )
    {
      refframe_array = ((img->type==B_IMG || img->type==BS_IMG) ? fw_refFrArr_frm : refFrArr_frm);
    }
    else if ( !img->top_field )
    {
      refframe_array = ((img->type==B_IMG || img->type==BS_IMG) ? fw_refFrArr_bot : refFrArr_bot);
      block_y        = ( img->block_y - 4 ) / 2;
    }
    else
    {
      refframe_array = ((img->type==B_IMG || img->type==BS_IMG) ? fw_refFrArr_top : refFrArr_top);
      block_y        = img->block_y / 2;
    }
  }

  if (currMB->mb_available[0][1] == NULL)
    b = 0;
  else
    b = (refframe_array[block_y+img->subblock_y-1][img->block_x+img->subblock_x] > 0 ? 1 : 0);
  if (currMB->mb_available[1][0] == NULL)
    a = 0;
  else 
    a = (refframe_array[block_y+img->subblock_y][img->block_x+img->subblock_x-1] > 0 ? 1 : 0);

  act_ctx     = a + 2*b; 
  se->context = act_ctx; // store context
  act_sym     = se->value1;

  if (act_sym==0)
  {
    biari_encode_symbol(eep_dp, 0, ctx->ref_no_contexts[addctx] + act_ctx );
  }
  else
  {
    biari_encode_symbol(eep_dp, 1, ctx->ref_no_contexts[addctx] + act_ctx);
    act_sym--;
    act_ctx=4;
    unary_bin_encode(eep_dp, act_sym,ctx->ref_no_contexts[addctx]+act_ctx,1);
  }
}


/*!
 ****************************************************************************
 * \brief
 *    This function is used to arithmetically encode the reference
 *    parameter of a given MB.
 ****************************************************************************
 */
void writeBwdRefFrame2Buffer_CABAC(SyntaxElement *se, EncodingEnvironmentPtr eep_dp)
{
  MotionInfoContexts  *ctx    = img->currentSlice->mot_ctx;
  Macroblock          *currMB = &img->mb_data[img->current_mb_nr];
  int                 addctx  = se->context;

  int   a, b;
  int   act_ctx;
  int   act_sym;
  int** refframe_array = ((img->type==B_IMG || img->type==BS_IMG)? bw_refFrArr : refFrArr);
  int   block_y        = img->block_y;


  if( input->InterlaceCodingOption >= MB_CODING && mb_adaptive )
  {
    if( !img->field_mode )
    {
      refframe_array = ((img->type==B_IMG || img->type==BS_IMG) ? bw_refFrArr_frm : refFrArr_frm);
    }
    else if ( !img->top_field )
    {
      refframe_array = ((img->type==B_IMG || img->type==BS_IMG) ? bw_refFrArr_bot : refFrArr_bot);
      block_y        = ( img->block_y - 4 ) / 2;
    }
    else
    {
      refframe_array = ((img->type==B_IMG || img->type==BS_IMG) ? bw_refFrArr_top : refFrArr_top);
      block_y        = img->block_y / 2;
    }
  }

  if (currMB->mb_available[0][1] == NULL)
    b = 0;
  else
    b = (BWD_IDX(refframe_array[block_y+img->subblock_y-1][img->block_x+img->subblock_x]) > 0 ? 1 : 0);
  if (currMB->mb_available[1][0] == NULL)
    a = 0;
  else 
    a = (BWD_IDX(refframe_array[block_y+img->subblock_y][img->block_x+img->subblock_x-1]) > 0 ? 1 : 0);

  act_ctx     = a + 2*b;
  se->context = act_ctx; // store context
  act_sym     = se->value1;

  if (act_sym==0)
  {
    biari_encode_symbol(eep_dp, 0, ctx->bwd_ref_no_contexts[addctx] + act_ctx );
  }
  else
  {
    biari_encode_symbol(eep_dp, 1, ctx->bwd_ref_no_contexts[addctx] + act_ctx);
    act_sym--;
    act_ctx=4;
    unary_bin_encode(eep_dp, act_sym,ctx->bwd_ref_no_contexts[addctx]+act_ctx,1);
  }
}


/*!
 ****************************************************************************
 * \brief
 *    This function is used to arithmetically encode the motion
 *    vector data of a given MB.
 ****************************************************************************
 */
void writeMVD2Buffer_CABAC(SyntaxElement *se, EncodingEnvironmentPtr eep_dp)
{
  int i = img->subblock_x;
  int j = img->subblock_y;
  int a, b;
  int act_ctx;
  int act_sym;
  int mv_pred_res;
  int mv_local_err;
  int mv_sign;
  int k = se->value2; // MVD component

  MotionInfoContexts  *ctx         = img->currentSlice->mot_ctx;
  Macroblock          *currMB      = &img->mb_data[img->current_mb_nr];

  if (j==0)
  {
    if (currMB->mb_available[0][1] == NULL)
      b = 0;
    else 
      b = absm((currMB->mb_available[0][1])->mvd[0][BLOCK_SIZE-1][i][k]);
  }
  else
    b = absm(currMB->mvd[0][j-1][i][k]);
          
  if (i==0)
  {
    if (currMB->mb_available[1][0] == NULL)
      a = 0;
    else 
      a = absm((currMB->mb_available[1][0])->mvd[0][j][BLOCK_SIZE-1][k]);
  }
  else
    a = absm(currMB->mvd[0][j][i-1][k]);

	se->value2 = a+b;
  if ((mv_local_err=a+b)<3)
    act_ctx = 5*k;
  else
  {
    if (mv_local_err>32)
      act_ctx=5*k+3;
    else
      act_ctx=5*k+2;
  }
  mv_pred_res = se->value1;
  se->context = act_ctx;

  act_sym = absm(mv_pred_res);

  if (act_sym == 0)
    biari_encode_symbol(eep_dp, 0, &ctx->mv_res_contexts[0][act_ctx] );
  else
  {
    biari_encode_symbol(eep_dp, 1, &ctx->mv_res_contexts[0][act_ctx] );
    mv_sign = (mv_pred_res<0) ? 1: 0;
    act_ctx=5*k+4;
    biari_encode_symbol_eq_prob(eep_dp, (unsigned char) mv_sign);
    act_sym--;
    act_ctx=5*k;
    unary_exp_golomb_mv_encode(eep_dp,act_sym,ctx->mv_res_contexts[1]+act_ctx,3);
  }
}

/*!
 ****************************************************************************
 * \brief
 *    This function is used to arithmetically encode the coded
 *    block pattern of a given delta quant.
 ****************************************************************************
 */
void writeDquant_CABAC(SyntaxElement *se, EncodingEnvironmentPtr eep_dp)
{
  MotionInfoContexts *ctx = img->currentSlice->mot_ctx;
  Macroblock *currMB = &img->mb_data[img->current_mb_nr];

  int act_ctx;
  int act_sym;
  int dquant = se->value1;
  int sign=0;

  if (dquant <= 0)
    sign = 1;
  act_sym = abs(dquant) << 1;

  act_sym += sign;
  act_sym --;

  if (currMB->mb_available[1][0] == NULL)
    act_ctx = 0;
  else
    act_ctx = ( ((currMB->mb_available[1][0])->delta_qp != 0) ? 1 : 0);

  if (act_sym==0)
  {
    biari_encode_symbol(eep_dp, 0, ctx->delta_qp_contexts + act_ctx );
  }
  else
  {
    biari_encode_symbol(eep_dp, 1, ctx->delta_qp_contexts + act_ctx);
    act_ctx=2;
    act_sym--;
    unary_bin_encode(eep_dp, act_sym,ctx->delta_qp_contexts+act_ctx,1);
  }
}

/*!
 ****************************************************************************
 * \brief
 *    This function is used to arithmetically encode the motion
 *    vector data of a B-frame MB.
 ****************************************************************************
 */
void writeBiMVD2Buffer_CABAC(SyntaxElement *se, EncodingEnvironmentPtr eep_dp)
{
  int i = img->subblock_x;
  int j = img->subblock_y;
  int a, b;
  int act_ctx;
  int act_sym;
  int mv_pred_res;
  int mv_local_err;
  int mv_sign;
  int backward = se->value2 & 0x01;
  int k = (se->value2>>1); // MVD component

  MotionInfoContexts  *ctx    = img->currentSlice->mot_ctx;
  Macroblock          *currMB = &img->mb_data[img->current_mb_nr];

  if (j==0)
  {
    if (currMB->mb_available[0][1] == NULL)
      b = 0;
    else
      b = absm((currMB->mb_available[0][1])->mvd[backward][BLOCK_SIZE-1][i][k]);
  }
  else
    b = absm(currMB->mvd[backward][j-1][i][k]);

  if (i==0)
  {
    if (currMB->mb_available[1][0] == NULL)
      a = 0;
    else
      a = absm((currMB->mb_available[1][0])->mvd[backward][j][BLOCK_SIZE-1][k]);
  }
  else
    a = absm(currMB->mvd[backward][j][i-1][k]);

  if ((mv_local_err=a+b)<3)
    act_ctx = 5*k;
  else
  {
    if (mv_local_err>32)
      act_ctx=5*k+3;
    else
      act_ctx=5*k+2;
  }
  mv_pred_res = se->value1;
  se->context = act_ctx;

  act_sym = absm(mv_pred_res);

  if (act_sym == 0)
    biari_encode_symbol(eep_dp, 0, &ctx->mv_res_contexts[0][act_ctx] );
  else
  {
    biari_encode_symbol(eep_dp, 1, &ctx->mv_res_contexts[0][act_ctx] );
    mv_sign = (mv_pred_res<0) ? 1: 0;
    act_ctx=5*k+4;
    biari_encode_symbol_eq_prob(eep_dp, (unsigned char) mv_sign);
    act_sym--;
    act_ctx=5*k;
    unary_exp_golomb_mv_encode(eep_dp,act_sym,ctx->mv_res_contexts[1]+act_ctx,3);
  }
}


/*!
 ****************************************************************************
 * \brief
 *    This function is used to arithmetically encode the chroma
 *    intra prediction mode of an 8x8 block
 ****************************************************************************
 */ //GB
void writeCIPredMode2Buffer_CABAC(SyntaxElement *se, EncodingEnvironmentPtr eep_dp)
{
  TextureInfoContexts *ctx     = img->currentSlice->tex_ctx;
  Macroblock          *currMB  = &img->mb_data[img->current_mb_nr];
  int                 act_ctx,a,b;
  int                 act_sym  = se->value1;

  if (currMB->mb_available[0][1] == NULL) b = 0;
  else  b = ( ((currMB->mb_available[0][1])->c_ipred_mode != 0) ? 1 : 0);

  if (currMB->mb_available[1][0] == NULL) a = 0;
  else  a = ( ((currMB->mb_available[1][0])->c_ipred_mode != 0) ? 1 : 0);

  act_ctx = a+b;

  if (act_sym==0) biari_encode_symbol(eep_dp, 0, ctx->cipr_contexts + act_ctx );
  else
  {
    biari_encode_symbol(eep_dp, 1, ctx->cipr_contexts + act_ctx );
    unary_bin_max_encode(eep_dp,(unsigned int) (act_sym-1),ctx->cipr_contexts+3,0,2);
  }

//	biari_encode_symbol_eq_prob(eep_dp, (signed short) (act_sym & 0x1));
	//biari_encode_symbol_eq_prob(eep_dp, (signed short) (act_sym & 0x2));

//	if (img->type==B_IMG || img->type==BS_IMG)
//		printf(" %d",act_sym);


}


/*!
 ****************************************************************************
 * \brief
 *    This function is used to arithmetically encode the coded
 *    block pattern of an 8x8 block
 ****************************************************************************
 */
void writeCBP_BIT_CABAC (int b8, int bit, int cbp, Macroblock* currMB, int inter, EncodingEnvironmentPtr eep_dp)
{
  int a, b;

  //===== GET CONTEXT FOR CBP-BIT =====
  if (b8/2 == 0) // upper block is in upper macroblock
  {
    if (currMB->mb_available[0][1] == NULL)
      b = 0;
    else
      b = ((currMB->mb_available[0][1]->cbp & (1<<(b8+2))) == 0 ? 1 : 0);
  }
  else
    b   = ((cbp & (1<<(b8-2))) == 0 ? 1: 0);
  if (b8%2 == 0) // left block is in left macroblock
  {
    if (currMB->mb_available[1][0] == NULL)
      a = 0;
    else
      a = ((currMB->mb_available[1][0]->cbp & (1<<(b8+1))) == 0 ? 1 : 0);
  }
  else
    a   = ((cbp & (1<<(b8-1))) == 0 ? 1: 0);

  //===== WRITE BIT =====
  biari_encode_symbol (eep_dp, (unsigned char) bit,
                       img->currentSlice->tex_ctx->cbp_contexts[0] + a+2*b);

}

/*!
 ****************************************************************************
 * \brief
 *    This function is used to arithmetically encode the coded
 *    block pattern of a macroblock
 ****************************************************************************
 */
void writeCBP2Buffer_CABAC(SyntaxElement *se, EncodingEnvironmentPtr eep_dp)
{
  TextureInfoContexts *ctx = img->currentSlice->tex_ctx;
  Macroblock *currMB = &img->mb_data[img->current_mb_nr];

  int a, b;
  int curr_cbp_ctx, curr_cbp_idx;
  int cbp = se->value1; // symbol to encode
  int cbp_bit;
  int b8;

  for (b8=0; b8<4; b8++)
  {
    curr_cbp_idx = (currMB->b8mode[b8] == IBLOCK ? 0 : 1);
    writeCBP_BIT_CABAC (b8, cbp&(1<<b8), cbp, currMB, curr_cbp_idx, eep_dp);
  }

  if ( se->type == SE_CBP_INTRA )
    curr_cbp_idx = 0;
  else
    curr_cbp_idx = 1;

  // coding of chroma part
  b = 0;
  if (currMB->mb_available[0][1] != NULL)
    b = ((currMB->mb_available[0][1])->cbp > 15) ? 1 : 0;

  a = 0;
  if (currMB->mb_available[1][0] != NULL)
    a = ((currMB->mb_available[1][0])->cbp > 15) ? 1 : 0;

  curr_cbp_ctx = a+2*b;
  cbp_bit = (cbp > 15 ) ? 1 : 0;
  biari_encode_symbol(eep_dp, (unsigned char) cbp_bit, ctx->cbp_contexts[1] + curr_cbp_ctx );

  if (cbp > 15)
  {
    b = 0;
    if (currMB->mb_available[0][1] != NULL)
      if ((currMB->mb_available[0][1])->cbp > 15)
        b = (( ((currMB->mb_available[0][1])->cbp >> 4) == 2) ? 1 : 0);

    a = 0;
    if (currMB->mb_available[1][0] != NULL)
      if ((currMB->mb_available[1][0])->cbp > 15)
        a = (( ((currMB->mb_available[1][0])->cbp >> 4) == 2) ? 1 : 0);

    curr_cbp_ctx = a+2*b;
    cbp_bit = ((cbp>>4) == 2) ? 1 : 0;
    biari_encode_symbol(eep_dp, (unsigned char) cbp_bit, ctx->cbp_contexts[2] + curr_cbp_ctx );
  }
}


static const int maxpos       [] = {16, 15, 64, 32, 32, 16,  4, 15};
static const int c1isdc       [] = { 1,  0,  1,  1,  1,  1,  1,  0};

static const int type2ctx_bcbp[] = { 0,  1,  2,  2,  3,  4,  5,  6}; // 7
static const int type2ctx_map [] = { 0,  1,  2,  3,  4,  5,  6,  7}; // 8
static const int type2ctx_last[] = { 0,  1,  2,  3,  4,  5,  6,  7}; // 8
static const int type2ctx_one [] = { 0,  1,  2,  3,  3,  4,  5,  6}; // 7
static const int type2ctx_abs [] = { 0,  1,  2,  3,  3,  4,  5,  6}; // 7



/*!
 ****************************************************************************
 * \brief
 *    Write CBP4-BIT
 ****************************************************************************
 */
void write_and_store_CBP_block_bit (Macroblock* currMB, EncodingEnvironmentPtr eep_dp, int type, int cbp_bit)
{
#define BIT_SET(x,n)  ((int)(((x)&(1<<(n)))>>(n)))

  int y_ac        = (type==LUMA_16AC || type==LUMA_8x8 || type==LUMA_8x4 || type==LUMA_4x8 || type==LUMA_4x4);
  int y_dc        = (type==LUMA_16DC);
  int u_ac        = (type==CHROMA_AC && !img->is_v_block);
  int v_ac        = (type==CHROMA_AC &&  img->is_v_block);
  int u_dc        = (type==CHROMA_DC && !img->is_v_block);
  int v_dc        = (type==CHROMA_DC &&  img->is_v_block);
  int j           = (y_ac || u_ac || v_ac ? img->subblock_y : 0);
  int i           = (y_ac || u_ac || v_ac ? img->subblock_x : 0);
  int bit         = (y_dc ? 0 : y_ac ? 1+4*j+i : u_dc ? 17 : v_dc ? 18 : u_ac ? 19+2*j+i : 23+2*j+i);
  int ystep_back  = (y_ac ? 12 : u_ac || v_ac ? 2 : 0);
  int xstep_back  = (y_ac ?  3 : u_ac || v_ac ? 1 : 0);
  int ystep       = (y_ac ?  4 : u_ac || v_ac ? 2 : 0);
  int default_bit = (img->is_intra_block ? 1 : 0);
  int upper_bit   = default_bit;
  int left_bit    = default_bit;
  int ctx;


  //--- set bits for current block ---
  if (cbp_bit)
  {
    if (type==LUMA_8x8)
    {
      currMB->cbp_bits   |= (1<< bit   );
      currMB->cbp_bits   |= (1<<(bit+1));
      currMB->cbp_bits   |= (1<<(bit+4));
      currMB->cbp_bits   |= (1<<(bit+5));
    }
    else if (type==LUMA_8x4)
    {
      currMB->cbp_bits   |= (1<< bit   );
      currMB->cbp_bits   |= (1<<(bit+1));
    }
    else if (type==LUMA_4x8)
    {
      currMB->cbp_bits   |= (1<< bit   );
      currMB->cbp_bits   |= (1<<(bit+4));
    }
    else
    {
      currMB->cbp_bits   |= (1<<bit);
    }
  }

  if (type!=LUMA_8x8)
  {
    //--- get bits from neighbouring blocks ---
    if (j==0)
    {
      if (currMB->mb_available[0][1])
      {
        upper_bit = BIT_SET(currMB->mb_available[0][1]->cbp_bits,bit+ystep_back);
      }
    }
    else
    {
      upper_bit = BIT_SET(currMB->cbp_bits,bit-ystep);
    }
    if (i==0)
    {
      if (currMB->mb_available[1][0])
      {
        left_bit = BIT_SET(currMB->mb_available[1][0]->cbp_bits,bit+xstep_back);
      }
    }
    else
    {
      left_bit = BIT_SET(currMB->cbp_bits,bit-1);
    }
    ctx = 2*upper_bit+left_bit;

    //===== encode symbol =====
    biari_encode_symbol (eep_dp, (short)cbp_bit, img->currentSlice->tex_ctx->bcbp_contexts[type2ctx_bcbp[type]]+ctx);
  }
}




//===== position -> ctx for MAP =====
//--- zig-zag scan ----
static const int  pos2ctx_map8x8 [] = { 0,  1,  2,  3,  4,  5,  5,  4,  4,  3,  3,  4,  4,  4,  5,  5,
                                        4,  4,  4,  4,  3,  3,  6,  7,  7,  7,  8,  9, 10,  9,  8,  7,
                                        7,  6, 11, 12, 13, 11,  6,  7,  8,  9, 14, 10,  9,  8,  6, 11,
                                       12, 13, 11,  6,  9, 14, 10,  9, 11, 12, 13, 11 ,14, 10, 12, 14}; // 15 CTX
static const int  pos2ctx_map8x4 [] = { 0,  1,  2,  3,  4,  5,  7,  8,  9, 10, 11,  9,  8,  6,  7,  8,
                                        9, 10, 11,  9,  8,  6, 12,  8,  9, 10, 11,  9, 13, 13, 14, 14}; // 15 CTX
static const int  pos2ctx_map4x4 [] = { 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 14}; // 15 CTX
static const int* pos2ctx_map    [] = {pos2ctx_map4x4, pos2ctx_map4x4, pos2ctx_map8x8, pos2ctx_map8x4,
                                       pos2ctx_map8x4, pos2ctx_map4x4, pos2ctx_map4x4, pos2ctx_map4x4};
//--- interlace scan ----
static const int  pos2ctx_map8x8i[] = { 0,  1,  1,  2,  2,  3,  3,  4,  5,  6,  7,  7,  7,  8,  4,  5,
                                        6,  9, 10, 10,  8, 11, 12, 11,  9,  9, 10, 10,  8, 11, 12, 11,
                                        9,  9, 10, 10,  8, 11, 12, 11,  9,  9, 10, 10,  8, 13, 13,  9,
                                        9, 10, 10,  8, 13, 13,  9,  9, 10, 10, 14, 14, 14, 14, 14, 14}; // 15 CTX
static const int  pos2ctx_map8x4i[] = { 0,  1,  2,  3,  4,  5,  6,  3,  4,  5,  6,  3,  4,  7,  6,  8,
                                        9,  7,  6,  8,  9, 10, 11, 12, 12, 10, 11, 13, 13, 14, 14, 14}; // 15 CTX
static const int  pos2ctx_map4x8i[] = { 0,  1,  1,  1,  2,  3,  3,  4,  4,  4,  5,  6,  2,  7,  7,  8,
                                        8,  8,  5,  6,  9, 10, 10, 11, 11, 11, 12, 13, 13, 14, 14, 14}; // 15 CTX
static const int* pos2ctx_map_int[] = {pos2ctx_map4x4, pos2ctx_map4x4, pos2ctx_map8x8i,pos2ctx_map8x4i,
                                       pos2ctx_map4x8i,pos2ctx_map4x4, pos2ctx_map4x4, pos2ctx_map4x4};


//===== position -> ctx for LAST =====
static const int  pos2ctx_last8x8 [] = { 0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
                                         2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
                                         3,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  4,  4,  4,
                                         5,  5,  5,  5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  8}; //  9 CTX
static const int  pos2ctx_last8x4 [] = { 0,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,  2,
                                         3,  3,  3,  3,  4,  4,  4,  4,  5,  5,  6,  6,  7,  7,  8,  8}; //  9 CTX
static const int  pos2ctx_last4x4 [] = { 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15}; // 15 CTX
static const int* pos2ctx_last    [] = {pos2ctx_last4x4, pos2ctx_last4x4, pos2ctx_last8x8, pos2ctx_last8x4,
                                        pos2ctx_last8x4, pos2ctx_last4x4, pos2ctx_last4x4, pos2ctx_last4x4};




/*!
 ****************************************************************************
 * \brief
 *    Write Significance MAP
 ****************************************************************************
 */
void write_significance_map (Macroblock* currMB, EncodingEnvironmentPtr eep_dp, int type, int coeff[], int coeff_ctr)
{
  short k, sig, last;
  int   k0      = 0;
  int   k1      = maxpos[type]-1;


  if (!c1isdc[type])
  {
    k0++; k1++; coeff--;
  }

  for (k=k0; k<k1; k++) // if last coeff is reached, it has to be significant
  {
    sig   = (coeff[k]!=0 ? 1 : 0);

    if (img->pstruct)
      biari_encode_symbol  (eep_dp, sig,  img->currentSlice->tex_ctx->map_contexts [type2ctx_map [type]]+pos2ctx_map_int [type][k]);
    else
      biari_encode_symbol  (eep_dp, sig,  img->currentSlice->tex_ctx->map_contexts [type2ctx_map [type]]+pos2ctx_map     [type][k]);
    if (sig)
    {
      last = (--coeff_ctr==0 ? 1 : 0);

      biari_encode_symbol(eep_dp, last, img->currentSlice->tex_ctx->last_contexts[type2ctx_last[type]]+pos2ctx_last[type][k]);
      if (last)
      {
        return;
      }
    }
  }
}


/*!
 ****************************************************************************
 * \brief
 *    Write Levels
 ****************************************************************************
 */
void write_significant_coefficients (Macroblock* currMB, EncodingEnvironmentPtr eep_dp, int type, int coeff[])
{
  int   i;
  int   absLevel;
  int   ctx;
  short sign;
  short greater_one;
  int   c1 = 1;
  int   c2 = 0;

  for (i=maxpos[type]-1; i>=0; i--)
  {
    if (coeff[i]!=0)
    {
      if (coeff[i]>0) {absLevel =  coeff[i];  sign = 0;}
      else            {absLevel = -coeff[i];  sign = 1;}

      greater_one = (absLevel>1 ? 1 : 0);

      //--- if coefficient is one ---
      ctx = min(c1,4);
      biari_encode_symbol (eep_dp, greater_one, img->currentSlice->tex_ctx->one_contexts[type2ctx_one[type]] + ctx);

      if (greater_one)
      {
        ctx = min(c2,4);
        unary_exp_golomb_level_encode(eep_dp, absLevel-2, img->currentSlice->tex_ctx->abs_contexts[type2ctx_abs[type]] + ctx);
        c1 = 0;
        c2++;
      }
      else if (c1)
      {
        c1++;
      }
      biari_encode_symbol_eq_prob (eep_dp, sign);
    }
  }
}



/*!
 ****************************************************************************
 * \brief
 *    Write Block-Transform Coefficients
 ****************************************************************************
 */
void writeRunLevel2Buffer_CABAC (SyntaxElement *se, EncodingEnvironmentPtr eep_dp)
{
  static int  coeff[64];
  static int  coeff_ctr = 0;
  static int  pos       = 0;

  Macroblock* currMB    = &img->mb_data[img->current_mb_nr];
  int         i;

  //--- accumulate run-level information ---
  if (se->value1 != 0)
  {
    for (i=0; i<se->value2; i++) coeff[pos++] = 0; coeff[pos++] = se->value1; coeff_ctr++;
    return;
  }
  else
  {
    for (; pos<64; pos++) coeff[pos] = 0;
  }

  //===== encode CBP-BIT =====
  write_and_store_CBP_block_bit     (currMB, eep_dp, se->context, coeff_ctr>0?1:0);

  if (coeff_ctr>0)
  {
    //===== encode significance map =====
    write_significance_map          (currMB, eep_dp, se->context, coeff, coeff_ctr);

    //===== encode significant coefficients =====
    write_significant_coefficients  (currMB, eep_dp, se->context, coeff);
  }

  //--- reset counters ---
  pos = coeff_ctr = 0;
}




/*!
 ************************************************************************
 * \brief
 *    Unary binarization and encoding of a symbol by using
 *    one or two distinct models for the first two and all
 *    remaining bins
*
************************************************************************/
void unary_bin_encode(EncodingEnvironmentPtr eep_dp,
                      unsigned int symbol,
                      BiContextTypePtr ctx,
                      int ctx_offset)
{
  unsigned int l;
  BiContextTypePtr ictx;

  if (symbol==0)
  {
    biari_encode_symbol(eep_dp, 0, ctx );
    return;
  }
  else
  {
    biari_encode_symbol(eep_dp, 1, ctx );
    l=symbol;
    ictx=ctx+ctx_offset;
    while ((--l)>0)
      biari_encode_symbol(eep_dp, 1, ictx);
    biari_encode_symbol(eep_dp, 0, ictx);
  }
  return;
}

/*!
 ************************************************************************
 * \brief
 *    Unary binarization and encoding of a symbol by using
 *    one or two distinct models for the first two and all
 *    remaining bins; no terminating "0" for max_symbol
 *    (finite symbol alphabet)
 ************************************************************************
 */
void unary_bin_max_encode(EncodingEnvironmentPtr eep_dp,
                          unsigned int symbol,
                          BiContextTypePtr ctx,
                          int ctx_offset,
                          unsigned int max_symbol)
{
  unsigned int l;
  BiContextTypePtr ictx;

  if (symbol==0)
  {
    biari_encode_symbol(eep_dp, 0, ctx );
    return;
  }
  else
  {
    biari_encode_symbol(eep_dp, 1, ctx );
    l=symbol;
    ictx=ctx+ctx_offset;
    while ((--l)>0)
      biari_encode_symbol(eep_dp, 1, ictx);
    if (symbol<max_symbol)
        biari_encode_symbol(eep_dp, 0, ictx);
  }
  return;
}



/*!
 ************************************************************************
 * \brief
 *    Exp Golomb binarization and encoding
 ************************************************************************
 */
void exp_golomb_encode_eq_prob( EncodingEnvironmentPtr eep_dp,
                                unsigned int symbol,
                                int k) 
{
  while(1)
  {
    if (symbol >= (unsigned int)(1<<k))   
    {
      biari_encode_symbol_eq_prob(eep_dp, 1);   //first unary part
      symbol = symbol - (1<<k);
      k++;
    }
    else                  
    {
      biari_encode_symbol_eq_prob(eep_dp, 0);   //now terminated zero of unary part
      while (k--)                               //next binary part
        biari_encode_symbol_eq_prob(eep_dp, (unsigned char)((symbol>>k)&1)); 
      break;
    }
  }

  return;
}

/*!
 ************************************************************************
 * \brief
 *    Exp-Golomb for Level Encoding
*
************************************************************************/
void unary_exp_golomb_level_encode( EncodingEnvironmentPtr eep_dp,
                                    unsigned int symbol,
                                    BiContextTypePtr ctx)
{
  unsigned int l,k;
  unsigned int exp_start = 13; // 15-2 : 0,1 level decision always sent

  if (symbol==0)
  {
    biari_encode_symbol(eep_dp, 0, ctx );
    return;
  }
  else
  {
    biari_encode_symbol(eep_dp, 1, ctx );
    l=symbol;
    k=1;
    while (((--l)>0) && (++k <= exp_start))
      biari_encode_symbol(eep_dp, 1, ctx);
    if (symbol < exp_start) biari_encode_symbol(eep_dp, 0, ctx);
    else exp_golomb_encode_eq_prob(eep_dp,symbol-exp_start,0);
  }
  return;
}



/*!
 ************************************************************************
 * \brief
 *    Exp-Golomb for MV Encoding
*
************************************************************************/
void unary_exp_golomb_mv_encode(EncodingEnvironmentPtr eep_dp,
                                unsigned int symbol,
                                BiContextTypePtr ctx,
                                unsigned int max_bin)
{
  unsigned int l,k;
  unsigned int bin=1;
  BiContextTypePtr ictx=ctx;
  unsigned int exp_start = 8; // 9-1 : 0 mvd decision always sent

  if (symbol==0)
  {
    biari_encode_symbol(eep_dp, 0, ictx );
    return;
  }
  else
  {
    biari_encode_symbol(eep_dp, 1, ictx );
    l=symbol;
    k=1;
    ictx++;
    while (((--l)>0) && (++k <= exp_start))
    {
      biari_encode_symbol(eep_dp, 1, ictx  );
      if ((++bin)==2) ictx++;
      if (bin==max_bin) ictx++;
    }
    if (symbol < exp_start) biari_encode_symbol(eep_dp, 0, ictx);
    else exp_golomb_encode_eq_prob(eep_dp,symbol-exp_start,3);
  }
  return;
}

