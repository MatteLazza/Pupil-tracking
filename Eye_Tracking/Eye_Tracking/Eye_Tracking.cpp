#include<opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <stdio.h>
#include <map>
#include <windows.h>
#pragma comment(lib, "user32.lib")




using namespace std;
using namespace cv;


//Those parameters will be changed at the beginning
string capturePhaseKey;
string closeKey;
int screen_Resolution_Width;
int screen_Resolution_Height;
//Set the size of the window used for getting the eye. IMPORTANT: THIS MUST BE THE SAME SIZE AS THE BLACK NOISE IMAGE.
int captureWidth;
int captureHeight;
//Change if you inserted the black noise image.
string blackNoise_Name_Path;
bool blackNoiseInserted = true; //by default it's true, it will be changed if no image will be found
//DEBUG. Show the looking point while doing the setup of the parameters
bool pupilTracking;






//default values, do not change. some can be changed during execution. Got them by doing tests with parameters during executions
int windowCaptureTopX = 714;
int windowCaptureTopY = 348;
int windowCaptureSizeX = 845;
int windowCaptureSizeY = 480;
int blurTimesBeforeResize = 5;
int saturationValueSelected = 41;
int darkThreshHold = 158;
int resizeSize = 300; //set the size of windows used in computation
bool parametersShowed = false;
string textSelection = "Tweek the parameters";

//method for getting the index of the biggest value in vector
int getIndexOfBiggest(vector<int> v) {
	int biggestArea = -1;
	int biggestAreaIndex = -1;
	for (int i = 0; i < v.size(); i++)
	{
		if (v[i] > biggestArea) {
			biggestArea = v[i];
			biggestAreaIndex = i;
		}
	}
	return biggestAreaIndex;
}
Mat resizeNormalizeThreshold(Mat eyeROI) {
	//Resizing of the eye and blurring for reducing noise and getting the eye position by saturation
	Mat thresh;
	Mat thresh2;
	resize(eyeROI, eyeROI, Size(resizeSize, resizeSize), 0, 0, 1);
	for (int i = 0; i < blurTimesBeforeResize; i++) {
		blur(eyeROI, eyeROI, Size(10, 10));
	}
	blur(eyeROI, thresh, Size(10, 10));
	thresh = (0.036 * saturationValueSelected) * thresh; //tested best value was 36, multiplied by a custom setting

	Mat norm;
	normalize(thresh, norm, 0, 255, NORM_MINMAX);
	threshold(norm, thresh2, darkThreshHold, 255, THRESH_BINARY);
	//imshow("Normalized", norm);

	if (!parametersShowed) {
		namedWindow("Parameters", 1);
		createTrackbar("Saturation value", "Parameters", &saturationValueSelected, 255);
		createTrackbar("Dark value", "Parameters", &darkThreshHold, 255);
		parametersShowed = true;
	}

	Mat normView = norm;
	resize(normView, normView, Size(resizeSize*2, resizeSize*2), 0, 0, 1);
	putText(normView, textSelection, cv::Point(30, 30), cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(0, 255, 0), 2, false);
	imshow("Normalized view", normView);
	return thresh2;
}

int main()
{
	Mat frame; //initialize the frame captured
	VideoCapture cap; //initialize the video capture
	cap.open(0); //open the DEFAULT camera
	int phase = 0;
	int steps = 0;
	string key;
	Mat eyeROI;
	Mat thresh2;
	int leftPos;
	int rightPos;
	int topPos;
	int bottomPos;


	//First thing is to open the file config to get all the configs that has been set in there.
	fstream new_file;
	new_file.open("config.txt", ios::in);
	// Checking whether the file is open. If not, the file isn't available, so send an error.
	if (!new_file.is_open()) {
		cout << "Error while reading the config.txt" << endl;
		exit(1);
	}
	//if it is, continue by getting all the parameters from the congif.txt. They will all be after the =
	string sa;
	vector<string> params;
	while (getline(new_file, sa)) {
		// ss is an object of stringstream that references the S string.  
		stringstream ss(sa);
		string str;
		int i = 0; //we only want the 2° element of the string,
		// Use while loop to check the getline() function condition.  
		while (getline(ss, str, '=')) {
			if (i == 1)
				params.push_back(str);
			
			i++;
		}
	}
	// Close the file object.
	new_file.close();

	//check if the amount of parameters is correct
	if (params.size() != 8) {
		cout << "Not all parameters has been set. Check again config folder" << endl;
	}
	//now it's just about setting the values. They are predefinided, so just set them.
	capturePhaseKey = params[0];
	captureWidth = stoi(params[1]);
	captureHeight = stoi(params[2]);
	screen_Resolution_Width = stoi(params[3]);
	screen_Resolution_Height = stoi(params[4]);
	blackNoise_Name_Path = params[5];
	closeKey = params[6];
	if (params[7] == "true")
		pupilTracking = true;




	//first, we have to get the black image for noise
	//If has been set as true but no image found, send error
	Mat blackNoise = imread(blackNoise_Name_Path);
	if(blackNoise.empty())
	{
		cout << "WARNING: No black noise found at the path selected and with the given name. Black noise reduction will not be used." << endl;
		blackNoiseInserted = false;
	}


	//open parameters for eye window
	namedWindow("Window parameters", 1);
	createTrackbar("Window capture top x", "Window parameters", &windowCaptureTopX, captureWidth);
	createTrackbar("Window capture top y", "Window parameters", &windowCaptureTopY, captureHeight);
	createTrackbar("Window size x", "Window parameters", &windowCaptureSizeX, captureWidth);
	createTrackbar("Window size y", "Window parameters", &windowCaptureSizeY, captureHeight);



	//set the size of the capture as specified before
	cap.set(CAP_PROP_FRAME_WIDTH, captureWidth);
	cap.set(CAP_PROP_FRAME_HEIGHT, captureHeight);


	//if the camera isn't able to open/load any frame, give an error end terminate the program
	if (!cap.isOpened()) {
		cerr << "ERROR! Unable to open camera\n";
		return -1;
	}
	//main loop
	for (;;)
	{
		//wait for a new frame from camera and store it into 'frame'
		cap.read(frame);
		
		//If happen that the camera captured an empty frame, return an error and close
		if (frame.empty()) {
			cerr << "ERROR! blank frame grabbed\n";
			break;
		}

		//get the key pressed
		key = (char)cv::waitKey(30);


		
#pragma region Fix window x-y movement if go more/less than the other one
		if (windowCaptureSizeX < windowCaptureTopX) {
			int temp = windowCaptureTopX;
			windowCaptureTopX = windowCaptureSizeX;
			windowCaptureSizeX = temp;
		}

		if (windowCaptureTopX > windowCaptureSizeX) {
			int temp = windowCaptureTopX;
			windowCaptureSizeX = windowCaptureTopX;
			windowCaptureTopX = temp;
		}

		if (windowCaptureSizeY < windowCaptureTopY) {
			int temp = windowCaptureTopY;
			windowCaptureTopY = windowCaptureSizeY;
			windowCaptureSizeY = temp;
		}

		if (windowCaptureTopY > windowCaptureSizeY) {
			int temp = windowCaptureTopY;
			windowCaptureSizeY = windowCaptureTopY;
			windowCaptureTopY = temp;
		}

#pragma endregion
		//If the eye window is not 0, calculate pupil position
		if (windowCaptureSizeX - windowCaptureTopX != 0 && windowCaptureSizeY - windowCaptureTopY != 0)
		{
			if(blackNoiseInserted)
				subtract(frame, blackNoise, frame);
			if (steps == 0) {
				//All texts and frames must be draw at the end of calculation
				putText(frame, "Move the box to cover the eye", cv::Point(30, 30), cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(0, 255, 0), 2, false);
				rectangle(frame, Point(windowCaptureTopX, windowCaptureTopY), Point(windowCaptureSizeX, windowCaptureSizeY), (255, 0, 0), 2);
				imshow("Eye positioning", frame);
				if (key == capturePhaseKey) {
					destroyWindow("Eye positioning");
					destroyWindow("Window parameters");
					steps++;
				}
			}
			else
			{
				//if we have done the first step, this mean we have the ROI of the eye
				//Get the eyeROI given the window parameters
				eyeROI = frame(Rect(windowCaptureTopX, windowCaptureTopY, windowCaptureSizeX - windowCaptureTopX, windowCaptureSizeY - windowCaptureTopY));
				cvtColor(eyeROI, eyeROI, cv::COLOR_BGR2GRAY);
				thresh2 = resizeNormalizeThreshold(eyeROI);
				imshow("Darkened thresh", thresh2);
				if (key == capturePhaseKey && steps == 1) { //if the user press the key, he have done the step. So destroy all the windows
					destroyWindow("Darkened thresh");
					destroyWindow("Normalized view");
					destroyWindow("Parameters");
					steps++;
					key = -1;
					textSelection = "Look left and press save key";
				}



				//calculating all contours
				//after the eyeROI and all the computation, calculate the contours of the dark values
				vector<vector<Point>> contours;
				vector<vector<Point>> newContours;
				vector<Vec4i> hierarchy;
				findContours(thresh2, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE);
				Mat drawing = Mat::zeros(thresh2.size(), CV_8UC3);


				//remove all the contours that are touching the borders. they are useless and give no informations
				for (vector<Point> c : contours) {
					Rect box = boundingRect(c);
					if (box.x != 0 && box.y != 0 && box.x + box.width != resizeSize && box.y + box.height != resizeSize) {
						newContours.push_back(c);
					}
				}


				//at this point, all dark point that were touching the border has been eliminated.
				//the pupil should be always the lowest point on the ROI, because under the eye there is just skin that has been saturated.
				//so we get the corner that is the lowest (highest value on screen)
				vector<cv::Point> centers;
				int selectedIndex = -1;
				int lowestY = 0;
				for (int i = 0; i < newContours.size(); i++) {
					int calcTemp = contourArea(newContours[i]);
					Rect box = boundingRect(newContours[i]);
					int cy = box.y + box.height / 2;
					if(cy > lowestY) {
						lowestY = cy;
						selectedIndex = i;
					}
				}


				//Calculate the center of the pupil
				//This must be done only if the pupil have been found in 
				Rect box;
				Mat lookingPoint = Mat::zeros(thresh2.size(), CV_8UC3);
				Point pupilCenter = Point(-1, -1);
				if (selectedIndex > -1) {
					box = boundingRect(newContours[selectedIndex]);
					pupilCenter = Point(box.x + box.width / 2, box.y + box.height / 2);
					circle(lookingPoint, pupilCenter, 2, Scalar(255, 0, 0), 2);
				}
				if(pupilTracking)
					imshow("Looking point", lookingPoint);




				//Having the looking point, we start tracking the cursor
				//before tracking it directly, we must determinate where are the boundaries of the screen
				//mapping is not 1-1 between windows and screen, so before we have to determinate the left-right-top-bottom points
				if (key == capturePhaseKey && steps > 1) {
					switch (phase)
					{
					case 0:
						//check if calculations has returned the center of the pupil
						if (pupilCenter.x > -1 && pupilCenter.y > -1) {
							leftPos = pupilCenter.x;
							phase++;
							textSelection = "Look right and press save key";
						}
						break;
					case 1:
						//check if calculations has returned the center of the pupil
						if (pupilCenter.x > -1 && pupilCenter.y > -1) {
							rightPos = pupilCenter.x;
							phase++;
							textSelection = "Look top and press save key";
						}
						break;
					case 2:
						//check if calculations has returned the center of the pupil
						if (pupilCenter.x > -1 && pupilCenter.y > -1) {
							topPos = pupilCenter.y;
							phase++;
							textSelection = "Look down and press save key";
						}
						break;
					case 3:
						//check if calculations has returned the center of the pupil
						if (pupilCenter.x > -1 && pupilCenter.y > -1) {
							bottomPos = pupilCenter.y;
							phase++;
						}
						break;
					}
				}


				//if all set up has been done, we can procede by calculating the position of the cursor on the screen
				//note that no all positions will be 100% accurate,
				if (phase > 3) {
					int heightSize = bottomPos - topPos;
					int widthSize = leftPos - rightPos;
					int pupil_X = pupilCenter.x - rightPos;
					int pupil_Y = pupilCenter.y - topPos;

					double width_Percentage = screen_Resolution_Width / widthSize;
					pupil_X = screen_Resolution_Width -(pupil_X * width_Percentage);
					double height_Percentage = screen_Resolution_Height / heightSize;
					pupil_Y = pupil_Y * height_Percentage;

					SetCursorPos(pupil_X, pupil_Y);
				}
			}
		}
		

		//because waitKey return an integer of the input, we can use it to get if ESC has been pressed. if so, close the program
		if (key == closeKey) return 0;
	}
	// the camera will be deinitialized automatically in VideoCapture destructor
	return 0;
}