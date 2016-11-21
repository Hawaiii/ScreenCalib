//=============================================================================
// Copyright © 2008 Point Grey Research, Inc. All Rights Reserved.
//
// This software is the confidential and proprietary information of Point
// Grey Research, Inc. ("Confidential Information").  You shall not
// disclose such Confidential Information and shall use it only in
// accordance with the terms of the license agreement you entered into
// with PGR.
//
// PGR MAKES NO REPRESENTATIONS OR WARRANTIES ABOUT THE SUITABILITY OF THE
// SOFTWARE, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
// IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
// PURPOSE, OR NON-INFRINGEMENT. PGR SHALL NOT BE LIABLE FOR ANY DAMAGES
// SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING OR DISTRIBUTING
// THIS SOFTWARE OR ITS DERIVATIVES.
//=============================================================================
//=============================================================================
// $Id: ExtendedShutterEx.cpp,v 1.9 2009-12-08 18:58:36 soowei Exp $
//=============================================================================

#include "stdafx.h"
#include <bitset>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>

#include "FlyCapture2.h"
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

using namespace FlyCapture2;
using namespace std;

enum ExtendedShutterType
{
	NO_EXTENDED_SHUTTER,
	DRAGONFLY_EXTENDED_SHUTTER,
	GENERAL_EXTENDED_SHUTTER
};

void PrintBuildInfo()
{
	FC2Version fc2Version;
	Utilities::GetLibraryVersion( &fc2Version );

	ostringstream version;
	version << "FlyCapture2 library version: " << fc2Version.major << "." << fc2Version.minor << "." << fc2Version.type << "." << fc2Version.build;
	cout << version.str() << endl;

	ostringstream timeStamp;
	timeStamp << "Application build date: " << __DATE__ << " " << __TIME__;
	cout << timeStamp.str() << endl << endl;
}

void PrintCameraInfo( CameraInfo* pCamInfo )
{
	cout << endl;
	cout << "*** CAMERA INFORMATION ***" << endl;
	cout << "Serial number -" << pCamInfo->serialNumber << endl;
	cout << "Camera model - " << pCamInfo->modelName << endl;
	cout << "Camera vendor - " << pCamInfo->vendorName << endl;
	cout << "Sensor - " << pCamInfo->sensorInfo << endl;
	cout << "Resolution - " << pCamInfo->sensorResolution << endl;
	cout << "Firmware version - " << pCamInfo->firmwareVersion << endl;
	cout << "Firmware build time - " << pCamInfo->firmwareBuildTime << endl << endl;


}

void PrintError( Error error )
{
	error.PrintErrorTrace();
}

int readImages(const char *path, std::vector<cv::Mat> &imgs);
{
	imgs.clear();

	DIR* dirFile = opendir( path );
   	if ( dirFile ) 
   	{
      	struct dirent* hFile;
      	int errno = 0;
      	while (( hFile = readdir( dirFile )) != NULL ) 
      	{
         	if ( !strcmp( hFile->d_name, "."  )) continue;
         	if ( !strcmp( hFile->d_name, ".." )) continue;

         	// in linux hidden files all start with '.'
         	if ( gIgnoreHidden && ( hFile->d_name[0] == '.' )) continue;

         	// dirFile.name is the name of the file. Do whatever string comparison 
         	// you want here. Something like:
         	if ( strstr( hFile->d_name, ".png" )){
            	printf( "found an .png file: %s", hFile->d_name );
            	cv::Mat im = cv::imread(path + hFile->d_name + ".png");
            	imgs.push_back(im);
         	}

      	} 
    	closedir( dirFile );
    	cout << "Read " << imgs.size() << " png images" << endl;
    	return 0;
   	}
   	return -1;
}

int SetupCameras (std::vector<Camera> &cams, int nCam)
{
	PrintBuildInfo();

	// const int k_numImages = 5;

	Error error;

	BusManager busMgr;
	unsigned int numCameras;
	error = busMgr.GetNumOfCameras(&numCameras);
	if (error != PGRERROR_OK)
	{
		PrintError( error );
		return -1;
	}

	cout << "Number of cameras detected: " << numCameras << endl;

	if ( numCameras < nCam )
	{
		cout << "Insufficient number of cameras... exiting" << endl;
		return -1;
	}

	for (int i = 0; i < nCam; i++){

		cout << "Connecting " << i << "th camera" << endl;

		PGRGuid guid;
		error = busMgr.GetCameraFromIndex(i, &guid);
		if (error != PGRERROR_OK)
		{
			PrintError( error );
			return -1;
		}

		Camera cam;

		// Connect to a camera
		error = cam.Connect(&guid);
		if (error != PGRERROR_OK)
		{
			PrintError( error );
			return -1;
		}

		// Get the camera information
		CameraInfo camInfo;
		error = cam.GetCameraInfo(&camInfo);
		if (error != PGRERROR_OK)
		{
			PrintError( error );
			return -1;
		}

		PrintCameraInfo(&camInfo);

		// Check if the camera supports the FRAME_RATE property
		PropertyInfo propInfo;
		propInfo.type = FRAME_RATE;
		error = cam.GetPropertyInfo( &propInfo );
		if (error != PGRERROR_OK)
		{
			PrintError( error );
			return -1;
		}

		ExtendedShutterType shutterType = NO_EXTENDED_SHUTTER;

		if ( propInfo.present == true )
		{
			// Turn off frame rate

			Property prop;
			prop.type = FRAME_RATE;
			error = cam.GetProperty( &prop );
			if (error != PGRERROR_OK)
			{
				PrintError( error );
				return -1;
			}

			prop.autoManualMode = false;
			prop.onOff = false;

			error = cam.SetProperty( &prop );
			if (error != PGRERROR_OK)
			{
				PrintError( error );
				return -1;
			}

			shutterType = GENERAL_EXTENDED_SHUTTER;
		}
		else
		{
			// Frame rate property does not appear to be supported.
			// Disable the extended shutter register instead.
			// This is only applicable for Dragonfly.

			const unsigned int k_extendedShutter = 0x1028;
			unsigned int extendedShutterRegVal = 0;

			error = cam.ReadRegister( k_extendedShutter, &extendedShutterRegVal );
			if (error != PGRERROR_OK)
			{
				PrintError( error );
				return -1;
			}

			std::bitset<32> extendedShutterBS((int) extendedShutterRegVal );
			if ( extendedShutterBS[31] == true )
			{
				// Set the camera into extended shutter mode
				error = cam.WriteRegister( k_extendedShutter, 0x80020000 );
				if (error != PGRERROR_OK)
				{
					PrintError( error );
					return -1;
				}
			}
			else
			{
				cout << "Frame rate and extended shutter are not supported... exiting" << endl;
				return -1;
			}

			shutterType = DRAGONFLY_EXTENDED_SHUTTER;
		}

		// Set the shutter property of the camera
		Property prop;
		prop.type = SHUTTER;
		error = cam.GetProperty( &prop );
		if (error != PGRERROR_OK)
		{
			PrintError( error );
			return -1;
		}

		prop.autoManualMode = false;
		prop.absControl = true;

		const float k_shutterVal = 3000.0;
		prop.absValue = k_shutterVal;

		error = cam.SetProperty( &prop );
		if (error != PGRERROR_OK)
		{
			PrintError( error );
			return -1;
		}

		cout << "Shutter time set to " << fixed << setprecision(2) << k_shutterVal << "ms" << endl;

		// Enable timestamping
		EmbeddedImageInfo embeddedInfo;

		error = cam.GetEmbeddedImageInfo( &embeddedInfo );
		if ( error != PGRERROR_OK )
		{
			PrintError( error );
			return -1;
		}

		if ( embeddedInfo.timestamp.available != 0 )
		{
			embeddedInfo.timestamp.onOff = true;
		}

		error = cam.SetEmbeddedImageInfo( &embeddedInfo );
		if ( error != PGRERROR_OK )
		{
			PrintError( error );
			return -1;
		}

		cams[i] = cam;
	}
}

int PutAwayCameras(std::vector<Camera> &cams){
	int error;
	int cnt = 0;

	for (Camera cam : cams){
		cout << "Putting away " << cnt++ << "th camera..." << endl;

		// Stop capturing images
		error = cam.StopCapture();
		if (error != PGRERROR_OK)
		{
			PrintError( error );
			return -1;
		}

		// Set the camera back to its original state
		Property prop;
		
		prop.type = SHUTTER;
		error = cam.GetProperty( &prop );
		if (error != PGRERROR_OK)
		{
			PrintError( error );
			return -1;
		}

		prop.autoManualMode = true;

		error = cam.SetProperty( &prop );
		if (error != PGRERROR_OK)
		{
			PrintError( error );
			return -1;
		}

		if ( shutterType == GENERAL_EXTENDED_SHUTTER )
		{
			Property prop;
			prop.type = FRAME_RATE;
			error = cam.GetProperty( &prop );
			if (error != PGRERROR_OK)
			{
				PrintError( error );
				return -1;
			}

			prop.autoManualMode = true;
			prop.onOff = true;

			error = cam.SetProperty( &prop );
			if (error != PGRERROR_OK)
			{
				PrintError( error );
				return -1;
			}

		}
		else if ( shutterType == DRAGONFLY_EXTENDED_SHUTTER )
		{
			const unsigned int k_extendedShutter = 0x1028;
			unsigned int extendedShutterRegVal = 0;

			error = cam.ReadRegister( k_extendedShutter, &extendedShutterRegVal );
			if (error != PGRERROR_OK)
			{
				PrintError( error );
				return -1;
			}

			std::bitset<32> extendedShutterBS((int) extendedShutterRegVal );
			if ( extendedShutterBS[31] == true )
			{
				// Set the camera into extended shutter mode
				error = cam.WriteRegister( k_extendedShutter, 0x80000000 );
				if (error != PGRERROR_OK)
				{
					PrintError( error );
					return -1;
				}
			}
		}

		// Disconnect the camera
		error = cam.Disconnect();
		if (error != PGRERROR_OK)
		{
			PrintError( error );
			return -1;
		}
	}

	// cout << "Done! Press Enter to exit..." << endl;
	// cin.ignore();
	return 0;
}

int main(int argc, char** argv)
{
	// Load images from directory
	if (argc < 2) {
		cout << "correct usage: ./ScreenCalib <img_folder_path>" << endl;
	}
	int error;

	vector<cv::Mat> imgs;
	error = readImages(argv[2], imgs);
	if (error != 0){
		cout << "Failed to read in images. Quitting..."
		return -1;
	}

	// Setup camera
	int nCams = 2;
	vector<Camera> cams;
	error = SetupCameras(cams, nCams);
	if (error != 0){
		cout << "Failed to setup " << nCams << " cameras. Quitting..."
		return -1;
	}

	// Display image and capture on all cameras
	for (cv::Mat im : imgs) {
		cvNamedWindow("SCalib", CV_WINDOW_NORMAL);
    	cvSetWindowProperty("SCalib", CV_WND_PROP_FULLSCREEN, CV_WINDOW_FULLSCREEN);
    	cvShowImage("SCalib", im);

    	cv::waitKey(1000);

    	for (int i = 0; i < cams.size(); i++) {

    		Camera cam = cams[i];
    		int errror;

			// Start the camera
			error = cam.StartCapture();
			if (error != PGRERROR_OK)
			{
				PrintError( error );
				continue;
			}

			Image image;
			error = cam.RetrieveBuffer( &image );
			if (error != PGRERROR_OK)
			{
				PrintError( error );
				continue;
			}

			TimeStamp timestamp = image.GetTimeStamp();
			cout << "TimeStamp [" << timestamp.cycleSeconds << " " << timestamp.cycleCount << "]" << endl;

			Image rgbImage;
			image.Convert( FlyCapture2::PIXEL_FORMAT_BGR, &rgbImage );
			unsigned int rowBytes = (double)rgbImage.GetReceivedDataSize()/(double)rgbImage.GetRows();
			cv::Mat cvImage = cv::Mat(rgbImage.GetRows(), rgbImage.GetCols(), CV_8UC3, rgbImage.GetData(), rowBytes);
			string imgname = "cam" + to_string(i) + "at" + std::to_string(timestamp.cycleSeconds) + "_" + std::to_string(timestamp.cycleCount) + ".png";
			cv::imwrite(imgname, cvImage);

			cv::waitKey(1000);
		}
    }
	
	// Put away cameras
	error = PutAwayCameras(cams);
	if (error != 0){
		cout << "Failed to put away cameras! Quitting ..." << endl;
		return -1;
	}

	return 0;
}
