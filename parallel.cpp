#include <iostream>
#include <dirent.h>
#include <sys/types.h>
#include <errno.h>
#include <string>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstddef>
#include <chrono>
#include <omp.h>

using namespace std;

void print_error(string image_name, string error_message)
{
    std::cout << "[ERROR] (" << image_name << ") - " << error_message << "\n";
}


int main(int argc, char **argv)
{
    // Start counter of total execution of program.
    auto total_start = chrono::high_resolution_clock::now();

    /*       .--.      .-'.      .--.      .--.      .--.      .--.      .`-.      .--.
       :::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\
       '      `--'      `.-'      `--'      `--'      `--'      `-.'      `--'      `
                            STAGE 1 --- COMMAND PARSING 
             .--.      .-'.      .--.      .--.      .--.      .--.      .`-.      .--.
       :::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\
       '      `--'      `.-'      `--'      `--'      `--'      `-.'      `--'      `
    */

    // If the number of arguments is different than three, stop execution.
    if (argc != 4)
    {
        cerr << "Wrong format:\n"
             << "image-seq operation in_path out_path\n"
             << "operation: copy, gauss, sobel\n";
        return -1;
    }

    // If the first word is distinct from copy, gauss or sobel, stop execution.
    if (strcmp(argv[1], "copy") != 0 && strcmp(argv[1], "gauss") != 0 && strcmp(argv[1], "sobel") != 0)
    {
        cerr << "Unexpected operation: " << argv[1] << "\n"
             << "image-seq operation in_path out path\n "
             << "operation: copy, gauss, sobel\n";
        return -1;
    }

    // Check if input directory exists and is accesible.
    if (opendir(argv[2]) == NULL)
    {
        if (errno == EACCES)
        {
            cerr << "Input path: " << argv[2] << "\n"
                 << "Output path: " << argv[3] << "\n"
                 << "input directory " << argv[3] << " cannot be open"
                 << "\n"
                 << "image-seq operation in_path out_path\n"
                 << "operation: copy, gauss, sobel\n";
            return -1;
        }
        else if (errno == ENOENT)
        {
            cerr << "Input path: " << argv[2] << "\n"
                 << "Output path: " << argv[3] << "\n"
                 << "input directory " << argv[3] << " cannot access path\n"
                 << "image-seq operation in_path out_path\n"
                 << "operation: copy, gauss, sobel\n";
            return -1;
        }
    }

    // Check if output directory exists and is accesible.
    if (opendir(argv[3]) == NULL)
    {
        if (errno == EACCES)
        {
            cerr << "Input path: " << argv[2] << "\n"
                 << "Output path: " << argv[3] << "\n"
                 << "output directory " << argv[3] << " cannot be open\n"
                 << "image-seq operation in_path out_path\n"
                 << "operation: copy, gauss, sobel\n";
            return -1;
        }
        else if (errno == ENOENT)
        {
            cerr << "Input path: " << argv[2] << "\n"
                 << "Output path: " << argv[3] << "\n"
                 << "output directory " << argv[3] << " does not exist\n"
                 << "image-seq operation in_path out_path\n"
                 << "operation: copy, gauss, sobel\n";
            return -1;
        }
    }


    // Extract the required operation.
    bool gauss = (string)argv[1] == "gauss";
    bool sobel = (string)argv[1] == "sobel";

    /*       .--.      .-'.      .--.      .--.      .--.      .--.      .`-.      .--.
       :::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\
       '      `--'      `.-'      `--'      `--'      `--'      `-.'      `--'      `
                            STAGE 2 --- IMAGE EXTRACTION 
             .--.      .-'.      .--.      .--.      .--.      .--.      .`-.      .--.
       :::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\
     '      `--'      `.-'      `--'      `--'      `--'      `-.'      `--'      `
    */

    // Struct to store extracted picture.
    struct raw_image
    {
        string name;
        string output_file_path;
        string input_file_path;
        vector<char> raw_data;
    };

    // Open the input directory.
    DIR *dr;
    dr = opendir(argv[2]);

    // Print the input and output path.
    cout << "Input path: " << argv[2] << endl;
    cout << "Output path: " << argv[3] << endl;
    cout << endl;

    /*
     Get all the file pointes and store the in a vector.
     They are stored this way so that they can be divided among the threads.
    */
    vector<dirent*> files_th;
    struct dirent *f;
    while ((f = readdir(dr))) 
    {
        files_th.push_back(f);
    }
    
    /*
     Iterate over every file in the input directory.
     One image is processed per loop.
    */

    #pragma omp parallel for
    for (unsigned int ii = 0; ii < files_th.size(); ii++)
    {
        // Start the total time counter per image.
        auto global_start = chrono::high_resolution_clock::now();
        // Start counter for the load phase.
        auto load_start = chrono::high_resolution_clock::now();

        // Check that the file is not the same or upper directory.
        if ((strcmp(files_th[ii]->d_name, ".") != 0 && strcmp(files_th[ii]->d_name, "..")) != 0)
        {

            // Get the path of the file.
            std::string input_file_path = argv[2];
            std::string output_file_path = argv[3];

            // Check if the path has the last slash.
            if (input_file_path.back() != '/')
                input_file_path.append("/");
            if (output_file_path.back() != '/')
                output_file_path.append("/");

            // Add the target image name to the path.
            input_file_path += files_th[ii]->d_name;
            output_file_path += files_th[ii]->d_name;

            // Get the size of the file by moving the file pointer at the end.
            ifstream ifs(input_file_path, ios::binary | ios::ate);
            ifstream::pos_type size = ifs.tellg();

            // Create the raw image structure.
            raw_image raw_img;

            // Set the original file name.
            raw_img.output_file_path = output_file_path;
            raw_img.input_file_path = input_file_path;
            raw_img.name = files_th[ii]->d_name;

            //Reads the contents of the file into the raw image.
            ifs.seekg(0, ios::beg);
            raw_img.raw_data.resize(size);
            ifs.read(&raw_img.raw_data[0], size);


            /*
             The raw image struct will be converted into an image struct.
             This struct will be used till the end and contains all the image information.
            */

            struct image
            {
                string name;
                string output_file_path;
                string input_file_path;
                unsigned int size;
                unsigned int width;
                unsigned int height;
                unsigned int start_byte;
                unsigned char raw_header[54];
                vector<unsigned char> pixels;
            };

            image img;

            // Copy values from raw image to the new struct.
            img.output_file_path = raw_img.output_file_path;
            img.input_file_path = raw_img.input_file_path;
            img.name = raw_img.name;

            // Chech that images have BM header. Stop if they do not.
            if (!(raw_img.raw_data[0] == 'B' && raw_img.raw_data[1] == 'M'))
            {
                cerr << "The image" << raw_img.name << "is not a bmp file \n";
                continue;
            }

            // Get the number of planes.
            unsigned int num_planes = (((unsigned int)(unsigned char)raw_img.raw_data[27]) << 8) + (unsigned int)(unsigned char)raw_img.raw_data[26];

            // Get the point size.
            unsigned int point_size = (((unsigned int)(unsigned char)raw_img.raw_data[29]) << 8) + (unsigned int)(unsigned char)raw_img.raw_data[28];

            // Get the compression variable.
            unsigned int compression = (((unsigned int)(unsigned char)raw_img.raw_data[33]) << 24) + (((unsigned int)(unsigned char)raw_img.raw_data[32]) << 16) + (((unsigned int)(unsigned char)raw_img.raw_data[31]) << 8) + ((unsigned int)(unsigned char)raw_img.raw_data[30]);

            // Check number of planes is one.
            if (num_planes != 1)
            {
                print_error(img.output_file_path, " has more than one plane");
                continue;
            }

            // Check point size is 24.
            if (point_size != 24)
            {
                print_error(img.output_file_path, " does not have 24 bits");
                continue;
            }
            
            // Check the image compression is zero.
            if (compression != 0)
            {
                print_error(img.output_file_path, " compression is different from 0");
                continue;
            }

            // Get the image width.
            img.width = (((unsigned int)(unsigned char)raw_img.raw_data[21]) << 24)
                        + (((unsigned int)(unsigned char)raw_img.raw_data[20]) << 16) + (((unsigned int)(unsigned char)raw_img.raw_data[19]) << 8) + ((unsigned int)(unsigned char)raw_img.raw_data[18]);

            // Get the height of the image.
            img.height = (((unsigned int)(unsigned char)raw_img.raw_data[25]) << 24) + (((unsigned int)(unsigned char)raw_img.raw_data[24]) << 16) + (((unsigned int)(unsigned char)raw_img.raw_data[23]) << 8) + ((unsigned int)(unsigned char)raw_img.raw_data[22]);

            // Get the image start of data.
            img.start_byte = (((unsigned int)(unsigned char)raw_img.raw_data[13]) << 24) + (((unsigned int)(unsigned char)raw_img.raw_data[12]) << 16) + (((unsigned int)(unsigned char)raw_img.raw_data[11]) << 8) + ((unsigned int)(unsigned char)raw_img.raw_data[10]);

            // Extract the pixel array from the raw image.
            img.pixels.insert(img.pixels.begin(), raw_img.raw_data.begin() + img.start_byte, raw_img.raw_data.end());


            /*       .--.      .-'.      .--.      .--.      .--.      .--.      .`-.      .--.
            :::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\
             '      `--'      `.-'      `--'      `--'      `--'      `-.'      `--'      `
                                 STAGE 3 --- DECOMPOSER
                  .--.      .-'.      .--.      .--.      .--.      .--.      .`-.      .--.
            :::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\
          '      `--'      `.-'      `--'      `--'      `--'      `-.'      `--'      `
          */

            // Create vectors for storing the decompose image.
            vector<unsigned char> blue(img.height * img.width);
            vector<unsigned char> red(img.height * img.width);
            vector<unsigned char> green(img.height * img.width);

            // Calculate the padding the raw image has.
            int padding = 4 - ((img.width * 3) % 4);
            if (padding == 4)
                padding = 0;

            // This width includes the padding.
            int real_width = img.width * 3 + padding;

            // This part is compulsory in order to be able to apply gauss and sobel opeartions.
            if (gauss || sobel)
            {

                int real_index = 0;
                #pragma omp parallel for
                for (unsigned j = 0; j < img.pixels.size(); j++)
                {
                    // This variables does not count the padding.
                    real_index = j - ((j / real_width) * padding);

                    if (j % real_width >= img.width * 3)
                    {
                        continue;
                    }
                    // Red pixels.
                    else if (real_index % 3 == 0)
                    {
                        red[real_index/3] = (img.pixels[j]);
                    }
                    // Green pixels.
                    else if (real_index % 3 == 1)
                    {
                        green[real_index/3] = (img.pixels[j]);
                    }
                    // Blue pixels.
                    else
                    {
                        blue[real_index/3] = (img.pixels[j]);
                    }
                }
            }

            // The decomposer is included in the load operation.
            auto load_end = chrono::high_resolution_clock::now();

         /*       .--.      .-'.      .--.      .--.      .--.      .--.      .`-.      .--.
              :::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\
            '      `--'      `.-'      `--'      `--'      `--'      `-.'      `--'      `
                                    STAGE 4 --- GAUSS OPERATION 
                    .--.      .-'.      .--.      .--.      .--.      .--.      .`-.      .--.
               :::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\
           '      `--'      `.-'      `--'      `--'      `--'      `-.'      `--'      `
          */
            
            auto gauss_start = chrono::high_resolution_clock::now();

            //Gaussian blur matrix.
            int m[5][5] = {
                {1, 4, 7, 4, 1},
                {4, 16, 26, 16, 4},
                {7, 26, 41, 26, 7},
                {4, 16, 26, 16, 4},
                {1, 4, 7, 4, 1}};
            int weight = 273;

            //Sobel mask matrices.
            int mx[3][3] = {
                {1, 2, 1},
                {0, 0, 0},
                {-1, -2, -1}};

            int my[3][3] = {
                {-1, 0, 1},
                {-2, 0, 2},
                {-1, 0, 1}};
            int w = 8;

            // Vector to store changes after gauss operation.
            vector<unsigned char> red_copy(red.size());
            vector<unsigned char> blue_copy(blue.size());
            vector<unsigned char> green_copy(green.size());

            // Gauss is executed also if sobel is executed.
            if (gauss || sobel)
            {
                #pragma omp parallel for
                for (int j = 0; j < (int)red.size(); j++)
                {
                    int col = j % img.width;
                    int row = j / img.width;
                    int result_red = 0;
                    int result_blue = 0;
                    int result_green = 0;
                            
                    for (int s = -2; s < 3; s++)
                    {
                        for (int t = -2; t < 3; t++)
                        {
                            // Check that operations outside image are excluded, and therefore treated as if the result was zero.
                            if ((row + s >= 0) && (col + t >= 0) && (col + t < (int)img.width) && (row + s < (int)img.height))
                            {
                                result_red += m[s + 3 - 1][t + 3 - 1] * red[(row + s) * img.width + (col + t)];
                                result_blue += m[s + 3 - 1][t + 3 - 1] * blue[(row + s) * img.width + (col + t)];
                                result_green += m[s + 3 - 1][t + 3 - 1] * green[(row + s) * img.width + (col + t)];
                            }
                        }
                    }
                    
                    // The gauss execution results are stored in the <color>_copy vectors.
                    red_copy[j] = result_red / weight;
                    blue_copy[j] = result_blue / weight;
                    green_copy[j] = result_green / weight;
                }
            }

            auto gauss_end = chrono::high_resolution_clock::now();

           /*       .--.      .-'.      .--.      .--.      .--.      .--.      .`-.      .--.
             :::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\
             '      `--'      `.-'      `--'      `--'      `--'      `-.'      `--'      `
                                  STAGE 5 --- SOBEL OPERATION 
                  .--.      .-'.      .--.      .--.      .--.      .--.      .`-.      .--.
            :::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\
            '      `--'      `.-'      `--'      `--'      `--'      `-.'      `--'      `
           */

            auto sobel_start = chrono::high_resolution_clock::now();

            if (sobel)
            {
                float res_x_red;
                float res_x_blue;
                float res_x_green;

                float res_y_red;
                float res_y_blue;
                float res_y_green;

                // Sobel is completly parallelizable since operations on one pixel does not depend on previous ones.
                #pragma omp parallel for                
                for (int j = 0; j < (int)red.size(); j++)
                {
                    int col = j % img.width;
                    int row = j / img.width;
                    int result_red = 0;
                    int result_blue = 0;
                    int result_green = 0;

                    // First sobel mask (mx)
                    for (int s = -1; s < 2; s++)
                    {
                        for (int t = -1; t < 2; t++)
                        {

                            // Check that operations outside image are excluded, and therefore treated as if the result was zero.
                            if ((row + s >= 0) && (col + t >= 0) && (col + t < (int)img.width) && (row + s < (int)img.height))
                            {
                                result_red += mx[s + 2 - 1][t + 2 - 1] * red_copy[(row + s) * img.width + (col + t)];
                                result_blue += mx[s + 2 - 1][t + 2 - 1] * blue_copy[(row + s) * img.width + (col + t)];
                                result_green += mx[s + 2 - 1][t + 2 - 1] * green_copy[(row + s) * img.width + (col + t)];
                            }
                        }
                    }

                    res_x_red = (float) result_red /(float)  w;
                    res_x_blue = (float) result_blue / (float) w;
                    res_x_green = (float) result_green / (float) w;

                    result_red = 0;
                    result_blue = 0;
                    result_green = 0;

                    // Second sobel mask (my)
                    for (int s = -1; s < 2; s++)
                    {
                        for (int t = -1; t < 2; t++)
                        {

                            // Check that operations outside image are excluded, and therefore treated as if the result was zero.
                            if ((row + s >= 0) && (col + t >= 0) && (col + t < (int)img.width) && (row + s < (int)img.height))
                            {
                                result_red += my[s + 2 - 1][t + 2 - 1] * red_copy[(row + s) * img.width + (col + t)];
                                result_blue += my[s + 2 - 1][t + 2 - 1] * blue_copy[(row + s) * img.width + (col + t)];
                                result_green += my[s + 2 - 1][t + 2 - 1] * green_copy[(row + s) * img.width + (col + t)];
                            }
                        }
                    }

                    res_y_red = (float)result_red / (float)w;
                    res_y_blue = (float)result_blue / (float)w;
                    res_y_green = (float)result_green / (float)w;

                    // The results of sobel are stored in the original vector, unlike the gauss ones.
                    red[j] = static_cast<unsigned int>(abs(res_y_red) + abs(res_x_red));
                    blue[j] = static_cast<unsigned int>(abs(res_y_green) + abs(res_x_green));
                    green[j] = static_cast<unsigned int>(abs(res_y_blue) + abs(res_x_blue));
                }
            }

            auto sobel_end = chrono::high_resolution_clock::now();

            /*       .--.      .-'.      .--.      .--.      .--.      .--.      .`-.      .--.
               :::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\
               '      `--'      `.-'      `--'      `--'      `--'      `-.'      `--'      `
                                   STAGE 6 --- RECOMPOSER   
                   .--.      .-'.      .--.      .--.      .--.      .--.      .`-.      .--.
            :::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\
             '      `--'      `.-'      `--'      `--'      `--'      `-.'      `--'      `
          */

            // The recomposer time is considered to be part of the store time.
            auto store_start = chrono::high_resolution_clock::now();
            
            /*
             This vectors point to the vector that contain the results that will be recomposed.
             This was done to improve both performance and code legibility.
            */
            vector<unsigned char> *red_result;
            vector<unsigned char> *green_result;
            vector<unsigned char> *blue_result;

            /*
             Since sobel results are stored in the original vectors and gauss results are stored in the <color>_copy vectors.
             Pointers are used to get to the gauss results in _copy vectors (distinct from sobel ones).
            */
            if (gauss){
                red_result = &red_copy;
                green_result = &green_copy;
                blue_result = &blue_copy;
            }

            // Pointers are used to get to the gauss results in original  vectors (distinct from gauss ones).
            if(sobel){ 
                red_result = &red;
                green_result = &blue;
                blue_result = &green;
            }

            // Recomposition is performed and merges the three colour vectors into the original image pixels vector that was decomposed.
            if (gauss || sobel)
            {
                
                int real_index = 0;
                #pragma omp parallel for
                // The j index in this loop takes into account the padding.
                for (unsigned j = 0; j < img.pixels.size(); j++)
                {
                    // This variable accounts for the pixel number without taking into account padding.
                    real_index = j - ((j / real_width) * padding);

                    if (j % real_width >= img.width * 3)
                    {
                        continue;
                    }
                    // Red pixels.
                    else if (real_index % 3 == 0)
                    {
                        img.pixels[j] = (*red_result)[real_index/3];
                    }
                    // Green pixels.
                    else if (real_index % 3 == 1)
                    {
                        img.pixels[j] = (*green_result)[real_index/3];
                    }
                    // Blue pixels.
                    else
                    {
                        img.pixels[j] = (*blue_result)[real_index/3];
                    }
                }
                
            }
            

           /*       .--.      .-'.      .--.      .--.      .--.      .--.      .`-.      .--.
             :::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\
             '      `--'      `.-'      `--'      `--'      `--'      `-.'      `--'      `
                                       STAGE 7 --- COPY IMAGE TO FOLDER 
                   .--.      .-'.      .--.      .--.      .--.      .--.      .`-.      .--.
            :::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\::::::::.\
          '      `--'      `.-'      `--'      `--'      `--'      `-.'      `--'      `
          */


            unsigned int file_size = 54 + (unsigned int)img.pixels.size();
            unsigned int img_size = img.pixels.size();
            
            // Fill the final header.

            img.raw_header[0] = 'B';
            img.raw_header[1] = 'M';

            img.raw_header[2] = file_size;
            img.raw_header[3] = file_size >> 8;
            img.raw_header[4] = file_size >> 16;
            img.raw_header[5] = file_size >> 24;

            img.raw_header[6] = '\0';
            img.raw_header[7] = '\0';
            img.raw_header[8] = '\0';
            img.raw_header[9] = '\0';

            img.raw_header[10] = 54;
            img.raw_header[11] = '\0';
            img.raw_header[12] = '\0';
            img.raw_header[13] = '\0';

            img.raw_header[14] = 40;
            img.raw_header[15] = '\0';
            img.raw_header[16] = '\0';
            img.raw_header[17] = '\0';

            img.raw_header[18] = img.width;
            img.raw_header[19] = img.width >> 8;
            img.raw_header[20] = img.width >> 16;
            img.raw_header[21] = img.width >> 24;

            img.raw_header[22] = img.height;
            img.raw_header[23] = img.height >> 8;
            img.raw_header[24] = img.height >> 16;
            img.raw_header[25] = img.height >> 24;

            img.raw_header[26] = 1;
            img.raw_header[27] = '\0';

            img.raw_header[28] = 24;
            img.raw_header[29] = '\0';

            img.raw_header[30] = '\0';
            img.raw_header[31] = '\0';
            img.raw_header[32] = '\0';
            img.raw_header[33] = '\0';

            img.raw_header[34] = img_size;
            img.raw_header[35] = img_size >> 8;
            img.raw_header[36] = img_size >> 16;
            img.raw_header[37] = img_size >> 24;

            img.raw_header[38] = 19;
            img.raw_header[39] = 11;
            img.raw_header[40] = '\0';
            img.raw_header[41] = '\0';

            img.raw_header[42] = 19;
            img.raw_header[43] = 11;
            img.raw_header[44] = '\0';
            img.raw_header[45] = '\0';

            img.raw_header[46] = '\0';
            img.raw_header[47] = '\0';
            img.raw_header[48] = '\0';
            img.raw_header[49] = '\0';

            img.raw_header[50] = '\0';
            img.raw_header[51] = '\0';
            img.raw_header[52] = '\0';
            img.raw_header[53] = '\0';

            ofstream file(img.output_file_path);

            // Write the header to the file.
            for (unsigned char j : img.raw_header)
            {
                file << j;
            }

            // Write the pixels to the file.
            file.write((const char *) &img.pixels[0], img.pixels.size());

            file.close();
            
            // Finished storing the file.
            auto store_end = chrono::high_resolution_clock::now();
            auto global_end = chrono::high_resolution_clock::now();

            auto load_time = chrono::duration_cast<chrono::microseconds>(load_end - load_start).count();
            auto sobel_time = chrono::duration_cast<chrono::microseconds>(sobel_end - sobel_start).count();
            auto gauss_time = chrono::duration_cast<chrono::microseconds>(gauss_end - gauss_start).count();
            auto store_time = chrono::duration_cast<chrono::microseconds>(store_end - store_start).count();
            auto global_time = chrono::duration_cast<chrono::microseconds>(global_end - global_start).count();

            // Print the image processing times.
            cout << "File: " << img.input_file_path << " (time: " << global_time << ")" << endl;
            cout << "Load time: " << load_time << endl;
            cout << "Gauss time: " << gauss_time << endl;
            cout << "Sobel time: " << sobel_time << endl;
            cout << "Store time: " << store_time << endl;
            cout << endl;
        }
    }

    // Print the total time to process all the images.
    auto total_end = chrono::high_resolution_clock::now();
    auto total_time = chrono::duration_cast<chrono::milliseconds>(total_end - total_start).count();
    std::cerr << "" << (float)total_time/1000 << endl;

    closedir(dr); // Close the directory that was opened to read the images.
    
    return 0;
}
