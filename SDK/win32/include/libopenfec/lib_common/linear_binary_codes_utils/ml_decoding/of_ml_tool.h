/* $Id: of_ml_tool.h 205 2014-12-10 04:17:19Z roca $ */
/*
 * OpenFEC.org AL-FEC Library.
 * (c) Copyright 2009 - 2011 INRIA - All rights reserved
 * Contact: vincent.roca@inria.fr
 *
 * This software is governed by the CeCILL-C license under French law and
 * abiding by the rules of distribution of free software.  You can  use,
 * modify and/ or redistribute the software under the terms of the CeCILL-C
 * license as circulated by CEA, CNRS and INRIA at the following URL
 * "http://www.cecill.info".
 *
 * As a counterpart to the access to the source code and  rights to copy,
 * modify and redistribute granted by the license, users are provided only
 * with a limited warranty  and the software's author,  the holder of the
 * economic rights,  and the successive licensors  have only  limited
 * liability.
 *
 * In this respect, the user's attention is drawn to the risks associated
 * with loading,  using,  modifying and/or developing or reproducing the
 * software by the user in light of its specific status of free software,
 * that may mean  that it is complicated to manipulate,  and  that  also
 * therefore means  that it is reserved for developers  and  experienced
 * professionals having in-depth computer knowledge. Users are therefore
 * encouraged to load and test the software's suitability as regards their
 * requirements in conditions enabling the security of their systems and/or
 * data to be ensured and,  more generally, to use and operate it in the
 * same conditions as regards security.
 *
 * The fact that you are presently reading this means that you have had
 * knowledge of the CeCILL-C license and that you accept its terms.
 */

#ifndef OF_ML_TOOL
#define OF_ML_TOOL


#ifdef OF_USE_DECODER
#ifdef OF_USE_LINEAR_BINARY_CODES_UTILS
#ifdef ML_DECODING

#ifdef OF_DEBUG
#define OP_ARGS ,UINT32* op
#define OP_ARG_VAL ,&(ofcb->stats_xor->nb_xor_for_ML)
#else
#define OP_ARGS
#define OP_ARG_VAL
#endif


/**
 * This function solves the system: first triangularize the system, then for each column,
 * do a forward elimination, then do the backward elimination.
 *
 * @fn INT32			of_linear_binary_code_solve_dense_system (of_mod2dense *m,void ** constant_member,void **variables,of_linear_binary_code_cb_t *ofcb)
 * @brief			solves the system
 * @param m 			(IN/OUT) address of the dense matrix.
 * @param variables
 * @param constant_member	(IN/OUT)pointer to all constant members
 * @param ofcb			(IN/OUT) Linear-Binary-Code control-block.
 * @return			error status
 */
of_status_t
of_linear_binary_code_solve_dense_system (of_linear_binary_code_cb_t	*ofcb,
					  of_mod2dense		*m,
					  void			**constant_tab,
					  void			**variable_tab);


#endif //ML_DECODING				   
#endif //OF_USE_LINEAR_BINARY_CODES_UTILS
#endif //OF_USE_DECODER

#endif //OF_ML_TOOL
