/*
 * predict.c
 *
 * This module is the driver program for a variable order
 * finite context compression program.  The maximum order is
 * determined by command line option.  This particular version
 * also monitors compression ratios, and flushes the model whenever
 * the local (last 256 symbols) compression ratio hits 90% or higher.
 *
 * This code is based on the program comp-2.c, by Bob Nelson.
 * It has been adapted to do predictions instead of compression.
 * It builds the variable order markov model, but doesn't output
 * an encoded bit stream.
 *
 * To build this program, see similar command lines for comp-2.c.
 *
 * Command line options:
 *
 *  -f text_file_name  [defaults to test.inp]
 *  -o order [defaults to 3 for model-2]
 *  -logloss test_file_name    	# Calculate average-log loss for the given test string.
 *  -p test_file_name			# Run a prediction for each char of given test string.
 *  -v							# verbose mode (prints extra info to stdout
 * -delimiters string_of_delimeters	# characters in this string are ignored in prediction results.
 * (The -delimeters option is not supported in the 16bit version)
 * -input_type representation_type		# denotes type of input.  If verbose is set, outputs change by representation used.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>		// for log10() function;
#include "coder.h"
#include "model.h"
//include <bitio.h>
#include "predict.h"
#include "string16.h"
#include "mapping.h"	// for ap mapping, ap neighbors, timeslot mapping

/*
 * The file pointers are used throughout this module.
 */
FILE *training_file;		// File containing string to train on.
FILE *test_file;			// File to test against (form future predictions)
char verbose = FALSE;		// if true, print out lots of info

//unsigned int str_delimiters[10];		// delimeters to ignore in prediction tests.
int  representation;		// specified with -input_type argument.


/*
 * The main procedure is similar to the main found in COMP-1.C.
 * It has to initialize the coder, the bit oriented I/O, the
 * standard I/O, and the model.  It then sits in a loop reading
 * input symbols and encoding them.  One difference is that every
 * 256 symbols a compression check is performed.  If the compression
 * ratio exceeds 90%, a flush character is encoded.  This flushes
 * the encoding model, and will cause the decoder to flush its model
 * when the file is being expanded.  The second difference is that
 * each symbol is repeatedly encoded until a succesfull encoding
 * occurs.  When trying to encode a character in a particular order,
 * the model may have to transmit an ESCAPE character.  If this
 * is the case, the character has to be retransmitted using a lower
 * order.  This process repeats until a succesful match is found of
 * the symbol in a particular context.  Usually this means going down
 * no further than the order -1 model.  However, the FLUSH and DONE
 * symbols do drop back to the order -2 model.  Note also that by
 * all rights, add_character_to_model() and update_model() logically
 * should be combined into a single routine.
 */
int main( int argc, char **argv )
{
     SYMBOL_TYPE c;
     int function;		// function to perform
     STRING16 * test_string;

     int i;				// general purpose register

    /* Initialize ********************************************/
    function = initialize_options( --argc, ++argv );
    initialize_model();
    test_string = string16(MAX_STRING_LENGTH+1);

    /* Train the model on the given input training file ***********/
    for ( ; ; )
    {
     		if (fread(&c, sizeof(SYMBOL_TYPE),1,training_file) == 0)
    			c = DONE;
    		/* NOTE: fread() seems to skip over whitespace chars, so be careful what's in your bin file. */
    		//printf("Training on 0x%04x\n", c);
            /*** The 16-bit version does not currently support delimiter-removal
            **  if (strchr(str_delimiters, c) != NULL)	{
    		** 	//printf("Skipping training on '%c'\n",c);
    		**	continue;					// This char is a delimiter, go onto the next character
    		** }
    		****************/
//       if ( c == EOF  || c == '\n' || c == 0x0a)	// skip line feeds
//            c = DONE;
       	clear_current_order();
        if ( c == DONE )
        	break;
        update_model( c );
        add_character_to_model( c );
    }

    /*** Print information about the model */
//    print_model_allocation();
//    if (verbose)
//    	print_model();
	/***************************************/

    /* Trying some probabilities....
	printf("PROBABILITIES\n");
	probability( 'r', "ab", verbose);
	probability( 'a', "ac", verbose);
	probability( 'a', "ad", verbose);
	probability( 'a', "br", verbose);
	probability( 'd', "ca", verbose);
	probability( 'b', "da", verbose);
	probability( 'c', "ra", verbose);
	probability( 'b', "a", verbose);
	probability( 'c', "a", verbose);
	probability( 'd', "a", verbose);
	probability( 'r', "b", verbose);
	probability( 'a', "c", verbose);
	probability( 'a', "d", verbose);
	probability( 'a', "r", verbose);
	probability('a',"", verbose);
	probability('r',"ac", verbose);
	probability( 'b', "a", verbose);
	probability( 'r', "ab", verbose);
*8*****/
//	probability( 'V', "01", verbose);

	/* Try some predictions
	printf("PREDICTIONS\n");

	// Try known predictions
	predict_next("ab", & pred);
	predict_next("br", & pred);
	predict_next("a", & pred);
	predict_next("b", & pred);
	predict_next("", & pred);

	// Try unseen context
	predict_next("ar", & pred);

	// Try an unknown symbol in the context
	predict_next("xy", & pred);

	******************************************/

	switch (function)	{
    	case PREDICT_TEST:
    		// read test file
    		i = fread16( test_string, MAX_STRING_LENGTH, test_file);
    		if (i == MAX_STRING_LENGTH)
    			fprintf(stderr,"Test String may be over max length and may have been truncated.\n");
    		predict_test(test_string);
    		break;
    	case LOGLOSS_EVAL:
    		// read test file
    		//fgets(test_string, MAX_STRING_LENGTH, test_file);
    		i = fread16( test_string, MAX_STRING_LENGTH, test_file);
    		if (i == MAX_STRING_LENGTH)
    			fprintf(stderr,"Test String may be over max length and may have been truncated.\n");
    		printf("%d, %f\n", max_order, compute_logloss(test_string, verbose));
    		break;
     	case NO_FUNCTION:
    	default:
    		break;
    	}
    exit( 0 );
}

/*
 * This routine checks for command line options, and opens the
 * input and output files.  The only other command line option
 * besides the input and output file names is the order of the model,
 * which defaults to 3.
 *
 * Returns the function to perform
 */
int initialize_options( int argc, char **argv )
{
    char training_file_name[ 81 ];
    char test_file_name[ 81 ];
    int function = NO_FUNCTION;
    char str_type[41];

    	/* Set Defaults */
#ifdef NOT_USED_IN_16_BIT_VERSION
    str_delimiters[0] = '\0';		// clear delimeter string
#endif
    str_type[0] = '\0';				// clear string_type
    representation = NONE;

//    strcpy( training_file_name, "test.inp" );
    while ( argc > 0 )    {
    	// -f <filename> gives the training file name
        if ( strcmp( *argv, "-f" ) == 0 ) 	{
        	argc--;
        	strcpy( training_file_name, *++argv );
        	if (verbose)
        		printf("Training on file %s\n", training_file_name);
        	}
        // -p <filename> gives the test filename to predict against
       else if ( strcmp( *argv, "-p" ) == 0 ) {
    	   	argc--;
    	   	strcpy( test_file_name, *++argv );
    	    test_file = fopen( test_file_name, "rb");
    	    if ( test_file == NULL )
    	    	{
    	        printf( "Had trouble opening the testing file (option -p)\n" );
    	        exit( -1 );
    	    	}
    	    setvbuf( test_file, NULL, _IOFBF, 4096 );
    	    function = PREDICT_TEST;
    	    if (verbose)
    	    	printf("Testing on file %s\n", test_file_name);
      		}
        // -o <order>
        else if ( strcmp( *argv, "-o" ) == 0 )
        	{
        	argc--;
            max_order = atoi( *++argv );
            // IN this version of the code, where we put time in context and
            // ask the model to predict loc (assuming time,loc pairs), the max_order
            // needs to be an odd number.
            if (max_order%2 == 0)
            	printf("max_order should be an odd value!\n");
        	}
        // -v
        else if ( strcmp( *argv, "-v" ) == 0 )
        	{
        	argc--;
            verbose = TRUE;			// print out prediction information
        	}
        // -logloss <test filename>
         else if ( strcmp( *argv, "-logloss" ) == 0 )
         	{
     	   	argc--;
     	   	strcpy( test_file_name, *++argv );
     	    test_file = fopen( test_file_name, "rb");
     	    if ( test_file == NULL )
     	    	{
     	        printf( "Had trouble opening the testing file (option -logloss)\n" );
     	        exit( -1 );
     	    	}
     	    setvbuf( test_file, NULL, _IOFBF, 4096 );
            function = LOGLOSS_EVAL;
         	}
#ifdef DELIMITER_CODE_NOT_SUPPORTED_IN_16BIT_MODEL
    	// -d <string> ignore prediction of delimeters in string
         else if ( strcmp( *argv, "-d" ) == 0 ) 	{
        	argc--;
        	strcpy( str_delimiters, *++argv );
        	if (verbose)
        		printf("Ignoring delimeters \"%s\"\n", str_delimiters);
        	}
#endif
    	// -input_type <string_type>  Indicate type of input strings used.
        // Choices include "locstrings", "boxstrings", "loctimestrings"
         else if ( strcmp( *argv, "-input_type" ) == 0 ) 	{
        	argc--;
        	strcpy( str_type, *++argv );
        	if (strcmp(str_type, "locstrings") == 0)
        		representation = LOCSTRINGS;
        	else if (strcmp(str_type, "loctimestrings")== 0)
        		representation = LOCTIMESTRINGS;
        	else if (strcmp(str_type, "boxstrings") == 0)
            	representation = BOXSTRINGS;
        	else if (strcmp(str_type, "binboxstrings") == 0)
            	representation = BINBOXSTRINGS;
        	else if (strcmp(str_type, "bindowts") == 0)
            	representation = BINDOWTS;
        	else
        		representation = NONE;
           	if (verbose)
       			printf("Input string type is %d (%s)\n",
        				representation,
        				str_representations[ representation]);
        	}
       else
        	{
            fprintf( stderr, "\nUsage: predict_MELT [-o order] [-v] [-logloss predictfile] " );
            fprintf( stderr, "[-f text file] [-p predictfile] [-input_type string_type]\n" );
            fprintf( stdout, "\nUsage: predict_MELT [-o order] [-v] [-logloss predictfile] " );
            fprintf( stdout, "[-f text file] [-p predictfile] [-input_type string_type]\n" );
             exit( -1 );
        	}
        argc--;
        argv++;
    	}
    training_file = fopen( training_file_name, "rb" );
    if (verbose)
    	fprintf(stdout,"%s\n", training_file_name);
    if ( training_file == NULL  )
    	{
        printf( "Had trouble opening the input training file %s!\n", training_file_name );
        exit( -1 );
    	}
    // Setup full buffering w/ a 4K buffer. (for speed)
    setvbuf( training_file, NULL, _IOFBF, 4096 );
    setbuf( stdout, NULL );
    return( function );
   }

/*******************************************
 * predict_test
 *
 * Given a test string, test each character of the string.
 * For example, if the test string is "abc", call
 * predict_next() for each substring:
 * 		predict_next("", &pred);
 * 		predict_next("a", &pred);
 * 		predict_next("ab", & pred);
 *
 * This version (predict_MELT) has been modified to use the
 * first character in each pair as context and the second to predict.
 * (Of course, this assumes 1st order, and a representation of
 * <time,loc> pairs (aka binboxstrings).)
 * INPUTS:
 * 	  test_string = pointer to string to test.
 *
 * RETURNS: nothing
 * *********************************************/
void predict_test( STRING16 * test_string){
	int i;			// index into test string
	int j;			// counter into # predictions
	int length;		// string length
	SYMBOL_TYPE predicted_char;
	int mapping;	// type of symbol (LOC, DUR, DELIM, etc)
	int num_tested=0, num_right=0;		// test results
    STRUCT_PREDICTION pred;
    int num_locations = 0;
    STRING16 *str_sub;		// sub-strings (chunks of context)
	int number_fallbacks_to_zero_but_still_right = 0;  // number of times it fell to level 0 and was still right.
	int total_fallbacks_to_zero = 0;	// number of times model went to level 0 for a prediction
	int number_multiple_predictions = 0; //number fo times model made > 1 prediction for a given time.
	int number_times_neighbors_are_correct = 0;	// number of times the prediction is a neighbor of actual
	unsigned char predicted_correctly;		// true if one of the predictions for a particular time are correct
	unsigned char is_neighbor;				// true if two aps are neighbors
	
	if (verbose)	{
    	printf("Testing on string %s\n", format_string16(test_string));
//    	if (strlen( str_delimiters))
//    		printf("ignoring characters in \"%s\"\n", str_delimiters);
       	printf("expected symbol, predicted symbol, # predictions, depth, probability, representation %s, is_neighbor\n",
       			str_representations[representation]);
    	}

    // initialize

    str_sub = string16(max_order);			// allocate memory for sub-string
    length = strlen16( test_string);

    // Go through test string, and try to predict every other symbol
    // using the context of the preceding symbol.
    // note that the loop starts with 1 because we are using char 0 for context only,
    // not prediction.
// original:    for (i=1; i < length; i+= 2, num_tested++)	{
    for (i=max_order; i < length; i+= 2, num_tested++)	{		// works for higher orders
		// Copy the symbols preceding the test-symbol into str_sub.
		if (i < max_order)
			strncpy16( str_sub, test_string,0, i);	// create test string
		else
			strncpy16( str_sub, test_string, i-max_order, max_order);

		//printf("predict_test: context_string is \"%s\", expected result is '0x%04x'\n",
			//	format_string16(str_sub), get_symbol( test_string, i));
		predicted_char = predict_next(str_sub, &pred);
		//printf("%s!\n\n", predicted_char == get_symbol( test_string, i) ?  "RIGHT" : "WRONG");
		// print
		// "expected, predicted, # predictions, depth, probability, symbol type"
		mapping = get_char_type( get_symbol( test_string, i), i);
		if (mapping != LOC)
			printf("Error: Expecting a LOC char and got something else! (0x%x)", get_symbol( test_string, i));		// It better be a LOC
		num_locations++;		// should equal num_tested in this case.
		if (pred.num_predictions > 1)
			number_multiple_predictions++;
		// Check each of the possible predictions
		predicted_correctly = FALSE;		// Assume they are all wrong.
		for (j=0; j < pred.num_predictions; j++)  {
			if (verbose) {
				if (get_symbol(test_string,i) == pred.sym[j].symbol)
					is_neighbor = FALSE;
				else 
					is_neighbor = neighboring_ap( pred.sym[j].symbol, get_symbol(test_string,i));
				printf("0x%04x, 0x%04x, %d, %d, %f, %s, %s\n",
					get_symbol( test_string, i),	// expected symbol
					pred.sym[j].symbol,				// predicted symbol
					pred.num_predictions,
					pred.depth,						// depth
					(float) pred.sym[j].prob_numerator/pred.prob_denominator,
					str_mappings[mapping],
					(is_neighbor) ? "YES" : "NO");
			}
			// if one of these predictions is right, increment the counter
			if (get_symbol( test_string,i) == pred.sym[j].symbol)	{
				num_right++;
				predicted_correctly = TRUE;		
				// Count the number of times it fell back to level 0 and still 
				// made a correct prediction.
				if (pred.depth == 0)
					number_fallbacks_to_zero_but_still_right++;
				}
			if (pred.depth == 0 && j == 0)	// count the number of level0 predictions, but
											// only count it once for multiple predictions
				total_fallbacks_to_zero++;
		}
		//If all the predictions are wrong, then check to see if one of the neighbors are right.
		if (!predicted_correctly) {
			for (j=0; j < pred.num_predictions; j++)  {
				if (neighboring_ap(pred.sym[j].symbol, get_symbol( test_string, i)))
					number_times_neighbors_are_correct++;
			}
		}
    }
	if (verbose)	{
		printf("max_order=%d, number of tests=%d, number correct=%d, %% correct = %.1f, number neighbors=%d\n",
				max_order,
				num_tested,
				num_right,
				num_tested ? 100 * (float) num_right/(float)num_tested : 0.0,
				number_times_neighbors_are_correct 	);
		}
	else
		/**** OLD WAY ****/
		/**printf("%d, %d, %d, %.1f, LOC: %d, %d, %d, %.1f, PAIRS: %d, %d\n",
			max_order,
			num_tested,
			num_right,
			100 * (float) num_right/(float)num_tested,
			max_order,
			num_locations,
			num_locations_right,
			num_locations ? 100 * (float) num_locations_right/(float) num_locations : 0.0,
			num_pairs_correct,
			num_locations);
			*****/
		/* Print only the percentage of pairs correct & percentage when time is correct */
		printf("%d, %d, %d, %.1f, %d, %d, %d, %d\n",
			max_order,				// should be '1'
			num_right,				// number of correct predictions
			num_locations,			// number of tests
			// percentage of correct predictions
			num_locations ? 100 * (float) num_right/(float) num_locations : 0.0,
			number_fallbacks_to_zero_but_still_right,		// number of times it fell back to level 0
									// but was still correct
			total_fallbacks_to_zero,	// number of times it fell back to 0, wrong or right prediction
			number_multiple_predictions,
			number_times_neighbors_are_correct); //# times pred was wrong but a neighbor was right.
	
	return;
}	// end of predict_test


/********************************************************************
 * get_char_type
 *
 * Return the type of character just predicted.
 * INPUTS: representation
 * 			expected symbol
 * 			index into input string
 * OUTPUTS: next_type_index is changed for loctimestrings
 * RETURNS: 0 = Location
 * 			1 = Starting Time
 * 			2 = Duration
 * 			3 = Delimiter
 ************************************************************************/
int get_char_type( SYMBOL_TYPE symbol, int index_into_input_string)
{
	switch (representation)	{
	case LOCSTRINGS:
		return( get_locstring_type( symbol));
	case BOXSTRINGS:
		return( get_boxstring_type( index_into_input_string));
	case LOCTIMESTRINGS:
		return( get_loctimestring_type( symbol));
	case BINBOXSTRINGS:
		return( get_binboxstring_type( symbol));
	case BINDOWTS:
		return( get_bindowts_type( symbol));
	default:	// If we don't know the string type, we can't figure out the character type
		return DELIM;
	}
}
/**************************************************************************
 * get_locstring_type
 * Given the character, return the character type (delimiter or location)
 *
 * INPUTS: symbol in input string
 * OUTPUTS: None
 * RETURNS: DELIM for a delimiter
 * 			LOC for a location char
 **************************************************************************/
int get_locstring_type( SYMBOL_TYPE symbol)
{
	if (symbol == (SYMBOL_TYPE) ':')
		return (DELIM);
	else
		return (LOC);
}

/**************************************************************************
 * get_boxstring_type
 * Given the index into the test string, return the character type (delimiter or location)
 *
 * INPUTS:  index into input string
 * OUTPUTS: None
 * RETURNS:
 * 			LOC for a location char
 * 			STRT for a starting time character
 * 			DUR	for a duration character
 **************************************************************************/
int get_boxstring_type( int index_into_input_string)
{
	switch (index_into_input_string % 6)
	{
	case 0: case 1:		return( STRT);
	case 2: case 3:		return( LOC);
	case 4: case 5:		return( DUR);
	default:	// Error!
		printf("Error in get_box_string_type\n");
		return -1;
	}
}

/**************************************************************************
 * get_loctimestring_type
 * Given the character (symbol),
 * return the character type (delimiter, location, etc)
 *
 * Loctimestrings look like this:
 *    L}tt:tt~dd:dd
 * where L is a location, tt:tt is the starting time and dd:dd is the duration.
 * INPUTS: 	symbol in input string
 *
 * OUTPUTS: None
 * RETURNS: DELIM for a delimiter
 * 			LOC for a location char
 **************************************************************************/
int get_loctimestring_type( SYMBOL_TYPE symbol)
{
	static int next_type_index = 0;
	static int types[] = {LOC, STRT, STRT, STRT, STRT, DUR, DUR, DUR, DUR};
	int result;

	if (symbol == (SYMBOL_TYPE) '}' || symbol == (SYMBOL_TYPE) ':' ||
			symbol ==  (SYMBOL_TYPE)'~' || symbol == (SYMBOL_TYPE) ';')
		return (DELIM);

	else {
		result = types[ next_type_index];
		next_type_index = (next_type_index + 1) % 9;	// point to the next type
		return(result);
	}
}

/**************************************************************************
 * get_binboxstring_type
 * Given the character, return the character type (delimiter or location)
 *
 * INPUTS: symbol in input string
 * OUTPUTS: None
 * RETURNS: DELIM for a delimiter
 * 			LOC for a location char
 **************************************************************************/
int get_binboxstring_type( SYMBOL_TYPE symbol)
{

	if (symbol >= INITIAL_START_TIME && symbol <= FINAL_START_TIME)
		return(STRT);
	if (symbol >= INITIAL_DURATION && symbol <= FINAL_DURATION)
		return(DUR);
	if (symbol >= INITIAL_LOCATION && symbol <= FINAL_LOCATION)
		return(LOC);

	// This is an error. We should never get here.
	return(DELIM);
}
/**************************************************************************
 * get_bindowts_type
 * Given the character, return the character type (delimiter or location)
 *
 * * The range of times is different for the DOWTS (day-of-week timeslot)
 * symbols.
 * INPUTS: symbol in input string
 * OUTPUTS: None
 * RETURNS: DELIM for a delimiter
 * 			LOC for a location char
 **************************************************************************/
int get_bindowts_type( SYMBOL_TYPE symbol)
{

	if (symbol >= INITIAL_START_TIME && symbol <= 0x25FF)
		return(STRT);
	if (symbol >= 0x2620 && symbol <= 0x26FF)
		return(LOC);

	// This is an error. We should never get here.
	return(DELIM);
}

#ifdef THIS_IS_THE_CODE_TO_TEST_THE_STRING16_ROUTINES

// TEST STRING16 routines
int main( int argc, char **argv )
{
	STRING16 * str16;
	int i;
	FILE * test_file;
	SYMBOL_TYPE c;

	str16 = string16(MAX_STRING_LENGTH);		// allocate new struct
	// Fill the string
/*	for (i = 0; i < 5; i++)
		str16->s[i] = (SYMBOL_TYPE) i + 0x1000;
	str16->s[i] = 0x0;
	str16->length = i+1;

	printf("The string is: %s\n", format_string16( str16));

	for (i=0; i < 5; i++)	{
		printf("The symbol at offset %d is 0x%04x.\n", i, get_symbol( str16, i));
	}

	for (i=0; i < 5; i++)	{
		shorten_string16( str16);
		printf("After shortening, the string is: %s\n", format_string16( str16));
	}
*/
	test_file = fopen( "108wks01_05.dat", "rb");
	/* This is one way to read in a file:  */
	i = fread16( str16, MAX_STRING_LENGTH, test_file);
	printf("The string is: %s\n", format_string16( str16));
	fclose( test_file);
	/**/

	/** And this is another **/
	test_file = fopen( "108wks01_05.dat", "r");
	do {
		//fscanf( test_file, "%x", &c);	// read an unsigned short integer (2 bytes)
		i = fread(&c, sizeof(SYMBOL_TYPE),1,test_file);
		if (i>0)
			printf("Training on 0x%04x\n", c);
	} while (i > 0);

	delete_string16( str16);
	exit(0);
}
#endif	// STRING16 Test Code

/***********************************************************
 *	neighboring_ap
 * 
 * Given two symbols for locations (AP) return true if
 * the first one is a neighbor of the second.
 * 
 ***********************************************************/
unsigned char neighboring_ap( SYMBOL_TYPE predicted_ap, SYMBOL_TYPE actual_ap)
{
	unsigned int i;
	unsigned int actual_ap_number;
	
	// Translate from the ap symbol value to the actual ap number (1-524)
	// mappings are in the ap_map[] in mapping.h
	for (i=0;i < 525; i++)
		if (ap_map[i] == actual_ap) {
			actual_ap_number = i;
			break;
		}
	if (i == 525)  {
		printf("Error: hit end of ap_map looking for 0x%x\n", actual_ap);
		return( FALSE );
	}
	
	// Look in the ap_neighbors array to see if the predicted_ap is a 
	// neighbor of the actual_ap.
	i = 0;
	while (ap_neighbors[ actual_ap_number][i] != 0) {
		//printf("compare 0x%x to 0x%x - ", ap_neighbors[ actual_ap_number][i], predicted_ap);
		if (ap_neighbors[ actual_ap_number][i] == predicted_ap) {
			//printf("return TRUE\n");
			return (TRUE);
		}
		//else printf("return FALSE\n");
		i++;
	}
	return(FALSE);
	
}	// end of neighboring_ap
