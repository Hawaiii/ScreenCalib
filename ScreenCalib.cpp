//=============================================================================
// Copyright � 2008 Point Grey Research, Inc. All Rights Reserved.
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
#include <stdio.h>
#include <cstdlib>
#include <fstream>
#include <dirent.h>


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

int readImages(const char *path, std::vector<cv::Mat> &imgs, std::vector<std::string> &img_names)
{
	imgs.clear();
	img_names.clear();

	DIR* dirFile = opendir( path );
   	if ( dirFile )
   	{
      	struct dirent* hFile;
      	while (( hFile = readdir( dirFile )) != NULL )
      	{
         	if ( !strcmp( hFile->d_name, "."  )) continue;
         	if ( !strcmp( hFile->d_name, ".." )) continue;

         	// in linux hidden files all start with '.'
         	if ( hFile->d_name[0] == '.' ) continue;

         	// dirFile.name is the name of the file. Do whatever string comparison
         	// you want here. Something like:
         	if ( strstr( hFile->d_name, ".bmp" )){
							char imname [2048];
							sprintf (imname, "%s%s", path, hFile->d_name);
							cout << "found an .bmp file: " << imname << endl;

            	cv::Mat im = cv::imread(imname, 1);
							img_names.push_back(hFile->d_name);
							imgs.push_back(im);

							cv::imshow("Name", im);
							cv::waitKey(250);
         	}

      	}
    	closedir( dirFile );
    	cout << "Read " << imgs.size() << " bmp images" << endl;
    	return 0;
   	}
   	return -1;
}

int SetupCameras (Camera **cams, uint nCam, std::vector<ExtendedShutterType> shutterTypes)
{
	// cams = new Camera* [nCam];
	PrintBuildInfo();

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

	for (uint i = 0; i < nCam; i++){

		cout << "Connecting " << i << "th camera" << endl;

		PGRGuid guid;
		error = busMgr.GetCameraFromIndex(i, &guid);
		if (error != PGRERROR_OK)
		{
			PrintError( error );
			return -1;
		}

		cams[i] = new Camera();

		// Connect to a camera
		error = cams[i]->Connect(&guid);
		if (error != PGRERROR_OK)
		{
			PrintError( error );
			return -1;
		}

		// Get the camera information
		CameraInfo camInfo;
		error = cams[i]->GetCameraInfo(&camInfo);
		if (error != PGRERROR_OK)
		{
			PrintError( error );
			return -1;
		}

		PrintCameraInfo(&camInfo);

		// Check if the camera supports the FRAME_RATE property
		PropertyInfo propInfo;
		propInfo.type = FRAME_RATE;
		error = cams[i]->GetPropertyInfo( &propInfo );
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
			error = cams[i]->GetProperty( &prop );
			if (error != PGRERROR_OK)
			{
				PrintError( error );
				return -1;
			}

			prop.autoManualMode = false;
			prop.onOff = false;

			error = cams[i]->SetProperty( &prop );
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

			error = cams[i]->ReadRegister( k_extendedShutter, &extendedShutterRegVal );
			if (error != PGRERROR_OK)
			{
				PrintError( error );
				return -1;
			}

			std::bitset<32> extendedShutterBS((int) extendedShutterRegVal );
			if ( extendedShutterBS[31] == true )
			{
				// Set the camera into extended shutter mode
				error = cams[i]->WriteRegister( k_extendedShutter, 0x80020000 );
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
		error = cams[i]->GetProperty( &prop );
		if (error != PGRERROR_OK)
		{
			PrintError( error );
			return -1;
		}
		prop.autoManualMode = false;
		prop.absControl = true;
		const float k_shutterVal = 100.0; //YI
		prop.absValue = k_shutterVal;
		error = cams[i]->SetProperty( &prop );
		if (error != PGRERROR_OK)
		{
			PrintError( error );
			return -1;
		}
		cout << "Shutter time set to " << fixed << setprecision(2) << prop.absValue << "ms" << endl;

		// YI: Set brightness
		prop.type = BRIGHTNESS;
		error = cams[i]->GetProperty( &prop );
		if (error != PGRERROR_OK){
			PrintError( error );
			return -1;
		}
		prop.autoManualMode = false;
		// prop.absControl = true;
		// const float k_brightnessVal = 1;
		// prop.absValue = k_brightnessVal;
		error = cams[i]->SetProperty( &prop );
		if (error != PGRERROR_OK)
		{
			PrintError( error );
			return -1;
		}
		cout << "Brightness set to " << fixed << setprecision(2) << prop.absValue << endl;

		// YI: Set autoexposure
		prop.type = AUTO_EXPOSURE;
		error = cams[i]->GetProperty( &prop );
		if (error != PGRERROR_OK){
			PrintError( error );
			return -1;
		}
		prop.autoManualMode = false;
		// prop.absControl = true;
		// const float k_exposureVal = 7;
		// prop.absValue = k_exposureVal;
		error = cams[i]->SetProperty( &prop );
		if (error != PGRERROR_OK)
		{
			PrintError( error );
			return -1;
		}
		cout << "Exposure set to " << fixed << setprecision(2) << prop.absValue << endl;

		// YI: Set gain
		prop.type = GAIN;
		error = cams[i]->GetProperty( &prop );
		if (error != PGRERROR_OK){
			PrintError( error );
			return -1;
		}
		prop.autoManualMode = false;
		prop.absControl = true;
		const float k_gainVal = 0;
		prop.absValue = k_gainVal;
		error = cams[i]->SetProperty( &prop );
		if (error != PGRERROR_OK)
		{
			PrintError( error );
			return -1;
		}
		cout << "Gain set to " << fixed << setprecision(2) << prop.absValue << endl;

		// Enable timestamping
		EmbeddedImageInfo embeddedInfo;

		error = cams[i]->GetEmbeddedImageInfo( &embeddedInfo );
		if ( error != PGRERROR_OK )
		{
			PrintError( error );
			return -1;
		}

		if ( embeddedInfo.timestamp.available != 0 )
		{
			embeddedInfo.timestamp.onOff = true;
		}

		error = cams[i]->SetEmbeddedImageInfo( &embeddedInfo );
		if ( error != PGRERROR_OK )
		{
			PrintError( error );
			return -1;
		}

		shutterTypes.push_back(shutterType);
	}

	for (uint i = 0; i < nCam; i++){
		CameraInfo camInfo;
		Error err;
		err = cams[i]->GetCameraInfo(&camInfo);
		if (err != PGRERROR_OK)
		{
			PrintError( err );
			return -1;
		}
    cout << "again " << i << endl;
		PrintCameraInfo(&camInfo);

	}
	return 0;
}

int PutAwayCameras(Camera **cams, uint nCams, std::vector<ExtendedShutterType> &shutterTypes){
	Error error;

	for (uint i = 0; i < nCams; i++){
		cout << "Putting away " << i << "th camera..." << endl;

		// Set the camera back to its original state
		Property prop;

		prop.type = SHUTTER;
		error = cams[i]->GetProperty( &prop );
		if (error != PGRERROR_OK)
		{
			PrintError( error );
			return -1;
		}

		prop.autoManualMode = true;

		error = cams[i]->SetProperty( &prop );
		if (error != PGRERROR_OK)
		{
			PrintError( error );
			return -1;
		}

		ExtendedShutterType shutterType = shutterTypes[i];
		if ( shutterType == GENERAL_EXTENDED_SHUTTER )
		{
			Property prop;
			prop.type = FRAME_RATE;
			error = cams[i]->GetProperty( &prop );
			if (error != PGRERROR_OK)
			{
				PrintError( error );
				return -1;
			}

			prop.autoManualMode = true;
			prop.onOff = true;

			error = cams[i]->SetProperty( &prop );
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

			error = cams[i]->ReadRegister( k_extendedShutter, &extendedShutterRegVal );
			if (error != PGRERROR_OK)
			{
				PrintError( error );
				return -1;
			}

			std::bitset<32> extendedShutterBS((int) extendedShutterRegVal );
			if ( extendedShutterBS[31] == true )
			{
				// Set the camera into extended shutter mode
				error = cams[i]->WriteRegister( k_extendedShutter, 0x80000000 );
				if (error != PGRERROR_OK)
				{
					PrintError( error );
					return -1;
				}
			}
		}

		// Disconnect the camera
		error = cams[i]->Disconnect();
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
	vector<string> img_names;
	error = readImages(argv[1], imgs, img_names);
	if (error != 0){
		cout << "Failed to read in images. Quitting..." << endl;
		return -1;
	}

	// Setup camera
	uint nCams = 2;
	Camera ** cams = new Camera* [nCams];
	vector<ExtendedShutterType> shutterTypes;
	error = SetupCameras(cams, nCams, shutterTypes);
	if (error != 0){
		cout << "Failed to setup " << nCams << " cameras. Quitting..." << endl;
		return -1;
	}

	Error err;
	for (uint i = 0; i < nCams; i++) {

		// Start the camera
		err = cams[i]->StartCapture();
		if (err != PGRERROR_OK)
		{
			PrintError( err );
			continue;
		}

	}

	cvNamedWindow("SCalib");
	// cvShowImage("SCalib", &im);
	cv::moveWindow("SCalib",-2160, 0);
	//cvSetWindowProperty("SCalib", CV_WND_PROP_FULLSCREEN, CV_WINDOW_FULLSCREEN);

	// Display image and capture on all cameras
	for (uint j = 0; j < imgs.size(); j++){
			cv::Mat im = imgs[j];
			cout<<im.rows<<" "<<im.cols<<endl;
			cv::imshow("SCalib", im);
			// if (j==0){
			// 	cv::waitKey(0);
			// }

    	cv::waitKey(1000);

    	for (uint i = 0; i < nCams; i++) {
					Image image;
					err = cams[i]->RetrieveBuffer( &image );
					if (err != PGRERROR_OK)
					{
						PrintError( err );
						continue;
					}
					string imgname = "out/" + to_string(i) + "_" + img_names[j];
					image.Save(imgname.c_str());
					// TimeStamp timestamp = image.GetTimeStamp();
					// cout << "TimeStamp [" << timestamp.cycleSeconds << " " << timestamp.cycleCount << "]" << endl;
					//
					// // Image rgbImage;
					// // image.Convert( FlyCapture2::PIXEL_FORMAT_BGR, &rgbImage );
					//
					// unsigned int rowBytes = (double)rgbImage.GetReceivedDataSize()/(double)rgbImage.GetRows();
					// cv::Mat cvImage = cv::Mat(rgbImage.GetRows(), rgbImage.GetCols(), CV_8UC3, rgbImage.GetData(), rowBytes);
					// string imgname = "out/" + to_string(i) + "_" + img_names[j];
					// cv::imwrite(imgname, cvImage);

			}
			cv::waitKey(1000);
  }

	for (uint i = 0; i < nCams; i++) {
		// Stop capturing images
		err = cams[i]->StopCapture();
		if (err != PGRERROR_OK)
		{
			PrintError( err );
			return -1;
		}
}


	// Put away cameras
	error = PutAwayCameras(cams, nCams, shutterTypes);
	if (error != 0){
		cout << "Failed to put away cameras! Quitting ..." << endl;
		return -1;
	}

	return 0;
}
