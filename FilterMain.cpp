#include <stdio.h>
#include "cs1300bmp.h"
#include <iostream>
#include <fstream>
#include "Filter.h"

using namespace std;

#include "rtdsc.h"

//
// Forward declare the functions
//
Filter * readFilter(string filename);
double applyFilter(Filter *filter, cs1300bmp *input, cs1300bmp *output);

int
main(int argc, char **argv)
{
    
    if ( argc < 2) {
        fprintf(stderr,"Usage: %s filter inputfile1 inputfile2 .... \n", argv[0]);
    }
    
    //
    // Convert to C++ strings to simplify manipulation
    //
    string filtername = argv[1];
    
    //
    // remove any ".filter" in the filtername
    //
    string filterOutputName = filtername;
    string::size_type loc = filterOutputName.find(".filter");
    if (loc != string::npos) {
        //
        // Remove the ".filter" name, which should occur on all the provided filters
        //
        filterOutputName = filtername.substr(0, loc);
    }
    
    Filter *filter = readFilter(filtername);
    
    double sum = 0.0;
    int samples = 0;
    
    for (int inNum = 2; inNum < argc; inNum++) {
        string inputFilename = argv[inNum];
        string outputFilename = "filtered-" + filterOutputName + "-" + inputFilename;
        struct cs1300bmp *input = new struct cs1300bmp;
        struct cs1300bmp *output = new struct cs1300bmp;
        int ok = cs1300bmp_readfile( (char *) inputFilename.c_str(), input);
        
        if ( ok ) {
            double sample = applyFilter(filter, input, output);
            sum += sample;
            samples++;
            cs1300bmp_writefile((char *) outputFilename.c_str(), output);
        }
        delete input;
        delete output;
    }
    fprintf(stdout, "Average cycles per sample is %f\n", sum / samples);
    
}

struct Filter *
readFilter(string filename)
{
    ifstream input(filename.c_str());
    
    if ( ! input.bad() ) {
        int size = 0;
        input >> size;
        Filter *filter = new Filter(size);
        int div;
        input >> div;
        filter -> setDivisor(div);
        for (int i=0; i < size; i++) {
            for (int j=0; j < size; j++) {
                int value;
                input >> value;
                filter -> set(i,j,value);
            }
        }
        return filter;
    }
}


double
applyFilter(struct Filter *filter, cs1300bmp *input, cs1300bmp *output)
{
    
    long long cycStart, cycStop;
    cycStart = rdtscll();
    
    int divisor = filter->getDivisor();
    int imgHeight = input->height;
    int imgWidth = input->width;
    int traversalHeight = imgHeight - 1;
    int traversalWidth = imgWidth - 1;
    
    output -> width = imgWidth;
    output -> height = imgHeight;
    
    int filterData[9] = {
        filter->get(0,0), filter->get(0,1), filter->get(0,2),
        filter->get(1,0), filter->get(1,1),	filter->get(1,2),
        filter->get(2,0), filter->get(2,1), filter->get(2,2)
    };

    int curr;
    
    //#pragma omp parallel for num_threads(3)
    for (int plane = 0; plane < 3; ++plane)
    {
		for (int row = 1; row < traversalHeight; ++row)
        
        {
            for (int col = 1; col < traversalWidth ; ++col)
            {
				
				// start keeps track of the pixel values
				//start keeps track of the pixel values
                int* pixelValue = &input->color[plane][row-1][col-1];
                int* filterValue = &filterData[0];
                curr = *(pixelValue++) * *(filterValue++); // [row-1][col-1]
                curr += *(pixelValue++) * *(filterValue++);    // [row-1][col]
                curr += *(pixelValue) * *(filterValue++);      // [row-1][col+1]
                pixelValue = &input->color[plane][row][col-1];
                curr += *(pixelValue++) * *(filterValue++);    // [row][col-1]
                curr += *(pixelValue++) * *(filterValue++);    // [row][col]
                curr += *(pixelValue) * *(filterValue++);      // [row][col+1]
                pixelValue = &input->color[plane][row+1][col-1];
                curr += *(pixelValue++) * *(filterValue++);    // [row+1][col-1]
                curr += *(pixelValue++) * *(filterValue++);		 // [row+1][col]
                curr += *(pixelValue) * *(filterValue);				 // [row+1][col+1]
                curr /= divisor;
                //?: more efficient than if/else
                curr = (curr > 255) ? 255 : curr;
                curr = (curr < 0) ? 0 : curr;
                output->color[plane][row][col] = curr;
            }
        }
    }
    
    cycStop = rdtscll();
    double diff = cycStop - cycStart;
    double diffPerPixel = diff / (output -> width * output -> height);
    fprintf(stderr, "Took %f cycles to process, or %f cycles per pixel\n",
            diff, diff / (output -> width * output -> height));
    return diffPerPixel;
}
