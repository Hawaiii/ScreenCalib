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

// #include "stdafx.h"
// #include <bitset>
#include <iostream>
#include <sstream>
// #include <iomanip>
#include <string>
#include <stdio.h>
#include <cstdlib>
#include <fstream>
#include <dirent.h>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
// Include files to use the PYLON API.
#include <pylon/PylonIncludes.h>
#include <pylon/usb/BaslerUsbInstantCamera.h>
#ifdef PYLON_WIN_BUILD
#    include <pylon/PylonGUI.h>
#endif

typedef Pylon::CBaslerUsbInstantCamera Camera_t;
using namespace Basler_UsbCameraParams;
using namespace Pylon;
using namespace std;


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
         	if ( strstr( hFile->d_name, ".png" )){
							// char imname [2048];
							// sprintf (imname, "%s%s", path, hFile->d_name);
							// cout << "found an .png file: " << imname << endl;

            	//cv::Mat im = cv::imread(imname, 1);
							img_names.push_back(hFile->d_name);
							//imgs.push_back(im);

							// cv::imshow("Name", im);
							// cv::waitKey(250);
         	}

      	}
    	closedir( dirFile );
    	cout << "Read " << imgs.size() << " png images" << endl;
    	return 0;
   	}
   	return -1;
}

int SetupCameras ( Camera_t **cams, uint nCam, Pylon::CImageFormatConverter &formatConverter)
{
	Pylon::PylonInitialize();
 	try{
			CDeviceInfo info;
			info.SetDeviceClass( Camera_t::DeviceClass());
			for (uint i = 0; i < nCam; i++){
					cout << "Setting up camera " << i << endl;
					cams[i] = new Camera_t( Pylon::CTlFactory::GetInstance().CreateFirstDevice( info ));
					cams[i]->Open();
					// Get camera device information.
	        cout << "Camera Device Information" << endl
	             << "=========================" << endl;
	        cout << "Vendor           : "
	             << cams[i]->DeviceVendorName.GetValue() << endl;
	        cout << "Model            : "
	             << cams[i]->DeviceModelName.GetValue() << endl;
	        cout << "Firmware version : "
	             << cams[i]->DeviceFirmwareVersion.GetValue() << endl << endl;

					cams[i]->GainSelector.SetValue(GainSelector_All);
					if (IsWritable(cams[i]->GainAuto)){
							cams[i]->GainAuto.SetValue(GainAuto_Off);
					}
					cams[i]->Gain.SetValue(0.0);

					cams[i]->ExposureAuto.SetValue(ExposureAuto_Off);
					cams[i]->ExposureTime.SetValue(33333.0);

					// cams[i]->BslColorSpaceMode.SetValue(BslColorSpaceMode_RGB);

					// cams[i]->LightSourcePreset.SetValue(LightSourcePreset_Off);
					// cams[i]->BalanceWhiteAuto.SetValue(BalanceWhiteAuto_Off);
					// cams[i]->BalanceRatioSelector.SetValue(BalanceRatioSelector_Red);
					// cams[i]->BalanceRatio.SetValue(1.0);
					// cams[i]->BalanceRatioSelector.SetValue(BalanceRatioSelector_Green);
					// cams[i]->BalanceRatio.SetValue(1.0);
					// cams[i]->BalanceRatioSelector.SetValue(BalanceRatioSelector_Blue);
					// cams[i]->BalanceRatio.SetValue(1.0);

					cams[i]->DefectPixelCorrectionMode.SetValue(DefectPixelCorrectionMode_Off);
			}
	} catch (const GenericException &e)
	{		// Error handling.
			cerr << "An exception occurred." << endl
			<< e.GetDescription() << endl;
			return -1;
	}
	// formatConverter.OutputPixelFormat = Pylon::PixelType_BGR8packed;
	formatConverter.OutputPixelFormat = Pylon::PixelType_Mono8;
	return 0;
}

int PutAwayCameras( Camera_t **cams, uint nCams){
	// TODO
	return -1;
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

	uint nCams = 1;
	Camera_t ** cams = new Camera_t *[nCams];
	Pylon::CGrabResultPtr ptrGrabResult;
	Pylon::CImageFormatConverter formatConverter;
	error = SetupCameras(cams, nCams, formatConverter);
	if (error != 0){
		cout << "Failed to setup " << nCams << " cameras. Quitting..." << endl;
		return -1;
	}

	cvNamedWindow("SCalib", CV_WINDOW_NORMAL);
	// cvShowImage("SCalib", &im);
	cv::moveWindow("SCalib",-2160, 0);
	cvSetWindowProperty("SCalib", CV_WND_PROP_FULLSCREEN, CV_WINDOW_FULLSCREEN);

	Pylon::CPylonImage pylonImage;
	cv::Mat openCvImage;

	// Display image and capture on all cameras
	for (uint k = 0; k < 3; k++){
		cout << "k: " << k << endl;
		for (uint j = 0; j < img_names.size(); j++){
			char imname [2048];
			sprintf (imname, "%s%s", argv[1], img_names[j].c_str());
			cv::Mat im = cv::imread(imname);
			cv::imshow("SCalib", im);
			cout << "j: " << j << endl;
    	//cv::waitKey(1000);

    	for (uint i = 0; i < nCams; i++) {

				// cv::waitKey(1000);
				try{
						cams[i]->GrabOne(500, ptrGrabResult, Pylon::TimeoutHandling_ThrowException);
						cv::waitKey(300);
						if (ptrGrabResult->GrabSucceeded()){
								formatConverter.Convert(pylonImage, ptrGrabResult);
								openCvImage = cv::Mat(ptrGrabResult->GetHeight(),
																			ptrGrabResult->GetWidth(),
																			CV_8UC1,
																			(uint8_t *)pylonImage.GetBuffer());
								string imgname = "out/" + to_string(i) + "_" + to_string(k) + "_" + img_names[j];
								try{
										cv::imwrite(imgname, openCvImage);
								} catch (runtime_error &ex){
										cout << "Error saving image!" << endl;
								}
						}
				} catch (const GenericException &e) {
						cerr << "An exception occurred." << endl
						<< e.GetDescription() << endl;
				}

			}//for i nCams
	}// for j imgs
}//k


	// Put away cameras
	error = PutAwayCameras(cams, nCams);
	if (error != 0){
		cout << "Failed to put away cameras! Quitting ..." << endl;
		return -1;
	}

	return 0;
}
