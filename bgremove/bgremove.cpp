// bgremove.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "bgremove.h"
#include <stdlib.h>
#include <string>
#include <algorithm>
#include <stdio.h>
#include <vector>



#include "opencv2/imgcodecs.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/videoio.hpp"
#include <opencv2/highgui.hpp>
#include <opencv2/video.hpp>
#include <opencv2/opencv.hpp>
#include<opencv2/core.hpp>



//C
#include <stdio.h>
//C++
//#include <iostream>
#include <sstream>

#include "base64.h"


using namespace std;
using namespace cv; 


static void help()
{
	/*cout << "\nThis program demonstrates GrabCut segmentation -- select an object in a region\n"
	"and then grabcut will attempt to segment it out.\n"
	"Call:\n"
	"./grabcut <image_name>\n"
	"\nSelect a rectangular area around the object you want to segment\n" <<
	"\nHot keys: \n"
	"\tESC - quit the program\n"
	"\tr - restore the original image\n"
	"\tn - next iteration\n"
	"\n"
	"\tleft mouse button - set rectangle\n"
	"\n"
	"\tCTRL+left mouse button - set GC_BGD pixels\n"
	"\tSHIFT+left mouse button - set GC_FGD pixels\n"
	"\n"
	"\tCTRL+right mouse button - set GC_PR_BGD pixels\n"
	"\tSHIFT+right mouse button - set GC_PR_FGD pixels\n" << endl;*/

	cout << "1- Draw a rectangle around the object to extract then press 'n'" << endl;
	cout << "2- Press 'n' again for 2nd pass." << endl;
	cout << "3- Press 'r' to resart scanning." << endl;
	cout << "3- Press 'esc' to quit." << endl;
}
const Scalar RED = Scalar(0, 0, 255);
const Scalar PINK = Scalar(230, 130, 255);
const Scalar BLUE = Scalar(255, 0, 0);
const Scalar LIGHTBLUE = Scalar(255, 255, 160);
const Scalar GREEN = Scalar(0, 255, 0);
const int BGD_KEY = EVENT_FLAG_CTRLKEY;
const int FGD_KEY = EVENT_FLAG_SHIFTKEY;
static void getBinMask(const Mat& comMask, Mat& binMask)
{
	if (comMask.empty() || comMask.type() != CV_8UC1)
		CV_Error(Error::StsBadArg, "comMask is empty or has incorrect type (not CV_8UC1)");
	if (binMask.empty() || binMask.rows != comMask.rows || binMask.cols != comMask.cols)
		binMask.create(comMask.size(), CV_8UC1);
	binMask = comMask & 1;
}
class GCApplication
{
public:
	enum { NOT_SET = 0, IN_PROCESS = 1, SET = 2 };
	static const int radius = 2;
	static const int thickness = -1;
	void reset();
	void setImageAndWinName(const Mat& _image, const string& _winName);
	Mat showImage() const;
	void mouseClick(int event, int x, int y, int flags, void* param);
	int nextIter();
	Mat bgRemove(Mat imgSource, int top, int left, int width, int height); //MKZ Added for BG Remove
	int getIterCount() const { return iterCount; }
private:
	void setRectInMask();
	void setLblsInMask(int flags, Point p, bool isPr);
	const string* winName;
	const Mat* image;
	Mat mask;
	Mat bgdModel, fgdModel;
	uchar rectState, lblsState, prLblsState;
	bool isInitialized;
	Rect rect;
	vector<Point> fgdPxls, bgdPxls, prFgdPxls, prBgdPxls;
	int iterCount;
};
void GCApplication::reset()
{
	if (!mask.empty())
		mask.setTo(Scalar::all(GC_BGD));
	bgdPxls.clear(); fgdPxls.clear();
	prBgdPxls.clear();  prFgdPxls.clear();
	isInitialized = false;
	rectState = NOT_SET;
	lblsState = NOT_SET;
	prLblsState = NOT_SET;
	iterCount = 0;
}
void GCApplication::setImageAndWinName(const Mat& _image, const string& _winName)
{
	if (_image.empty() || _winName.empty())
		return;
	image = &_image;
	winName = &_winName;
	mask.create(image->size(), CV_8UC1);
	reset();
}
Mat GCApplication::showImage() const
{
	Mat res;
	if (image->empty() || winName->empty())
		return res;

	Mat binMask;
	if (!isInitialized)
		image->copyTo(res);
	else
	{
		getBinMask(mask, binMask);
		image->copyTo(res, binMask);
	}
	vector<Point>::const_iterator it;
	for (it = bgdPxls.begin(); it != bgdPxls.end(); ++it)
		circle(res, *it, radius, BLUE, thickness);
	for (it = fgdPxls.begin(); it != fgdPxls.end(); ++it)
		circle(res, *it, radius, RED, thickness);
	for (it = prBgdPxls.begin(); it != prBgdPxls.end(); ++it)
		circle(res, *it, radius, LIGHTBLUE, thickness);
	for (it = prFgdPxls.begin(); it != prFgdPxls.end(); ++it)
		circle(res, *it, radius, PINK, thickness);
	//if (rectState == IN_PROCESS || rectState == SET)
		//rectangle(res, Point(rect.x, rect.y), Point(rect.x + rect.width, rect.y + rect.height), GREEN, 2);
	//imshow(*winName, res);
	return res;
}
void GCApplication::setRectInMask()
{
	CV_Assert(!mask.empty());
	mask.setTo(GC_BGD);
	rect.x = max(0, rect.x);
	rect.y = max(0, rect.y);
	rect.width = min(rect.width, image->cols - rect.x);
	rect.height = min(rect.height, image->rows - rect.y);
	(mask(rect)).setTo(Scalar(GC_PR_FGD));
}
void GCApplication::setLblsInMask(int flags, Point p, bool isPr)
{
	vector<Point> *bpxls, *fpxls;
	uchar bvalue, fvalue;
	if (!isPr)
	{
		bpxls = &bgdPxls;
		fpxls = &fgdPxls;
		bvalue = GC_BGD;
		fvalue = GC_FGD;
	}
	else
	{
		bpxls = &prBgdPxls;
		fpxls = &prFgdPxls;
		bvalue = GC_PR_BGD;
		fvalue = GC_PR_FGD;
	}
	if (flags & BGD_KEY)
	{
		bpxls->push_back(p);
		circle(mask, p, radius, bvalue, thickness);
	}
	if (flags & FGD_KEY)
	{
		fpxls->push_back(p);
		circle(mask, p, radius, fvalue, thickness);
	}
}
void GCApplication::mouseClick(int event, int x, int y, int flags, void*)
{
	// TODO add bad args check
	switch (event)
	{
	case EVENT_LBUTTONDOWN: // set rect or GC_BGD(GC_FGD) labels
	{
		bool isb = (flags & BGD_KEY) != 0,
			isf = (flags & FGD_KEY) != 0;
		if (rectState == NOT_SET && !isb && !isf)
		{
			rectState = IN_PROCESS;
			rect = Rect(x, y, 1, 1);
		}
		if ((isb || isf) && rectState == SET)
			lblsState = IN_PROCESS;
	}
	break;
	case EVENT_RBUTTONDOWN: // set GC_PR_BGD(GC_PR_FGD) labels
	{
		bool isb = (flags & BGD_KEY) != 0,
			isf = (flags & FGD_KEY) != 0;
		if ((isb || isf) && rectState == SET)
			prLblsState = IN_PROCESS;
	}
	break;
	case EVENT_LBUTTONUP:
		if (rectState == IN_PROCESS)
		{
			rect = Rect(Point(rect.x, rect.y), Point(x, y));
			rectState = SET;
			setRectInMask();
			CV_Assert(bgdPxls.empty() && fgdPxls.empty() && prBgdPxls.empty() && prFgdPxls.empty());
			showImage();
		}
		if (lblsState == IN_PROCESS)
		{
			setLblsInMask(flags, Point(x, y), false);
			lblsState = SET;
			showImage();
		}
		break;
	case EVENT_RBUTTONUP:
		if (prLblsState == IN_PROCESS)
		{
			setLblsInMask(flags, Point(x, y), true);
			prLblsState = SET;
			showImage();
		}
		break;
	case EVENT_MOUSEMOVE:
		if (rectState == IN_PROCESS)
		{
			rect = Rect(Point(rect.x, rect.y), Point(x, y));
			CV_Assert(bgdPxls.empty() && fgdPxls.empty() && prBgdPxls.empty() && prFgdPxls.empty());
			showImage();
		}
		else if (lblsState == IN_PROCESS)
		{
			setLblsInMask(flags, Point(x, y), false);
			showImage();
		}
		else if (prLblsState == IN_PROCESS)
		{
			setLblsInMask(flags, Point(x, y), true);
			showImage();
		}
		break;
	}
}
int GCApplication::nextIter()
{
	if (isInitialized)
		grabCut(*image, mask, rect, bgdModel, fgdModel, 1);
	else
	{
		if (rectState != SET)
			return iterCount;
		if (lblsState == SET || prLblsState == SET)
			grabCut(*image, mask, rect, bgdModel, fgdModel, 1, GC_INIT_WITH_MASK);
		else
			grabCut(*image, mask, rect, bgdModel, fgdModel, 1, GC_INIT_WITH_RECT);
		isInitialized = true;
	}
	iterCount++;
	bgdPxls.clear(); fgdPxls.clear();
	prBgdPxls.clear(); prFgdPxls.clear();
	return iterCount;
}

Mat GCApplication::bgRemove(Mat imgSource, int top, int left, int width, int height)
{
	rect = Rect(Point(top, left), Point(width, height));

	Mat image = imgSource;
	Mat ret;
	if (image.empty())
	{
		return imgSource;
	}
	const string winName = "image";
	//namedWindow(winName, WINDOW_AUTOSIZE);
	//setMouseCallback(winName, on_mouse, 0);
	setImageAndWinName(image, winName);
	ret = showImage();
	
	rectState = SET;
	setRectInMask();
	CV_Assert(bgdPxls.empty() && fgdPxls.empty() && prBgdPxls.empty() && prFgdPxls.empty());
	ret = showImage();

	int iterCount = getIterCount();
	int newIterCount = nextIter();
	if (newIterCount > iterCount)
	{
		ret = showImage();
	}

	return ret;
}

GCApplication gcapp;


extern "C" BGREMOVE_API char*  __stdcall  fnbgRemove(char* base64img,int top, int left, int width, int height)
{
	string encoded_string = base64img; 

	string decoded_string = base64_decode(encoded_string);
	vector<uchar> data(decoded_string.begin(), decoded_string.end());
	Mat img = imdecode(data, IMREAD_UNCHANGED);

	Mat dest;

	//imshow("Image", img);
	cv::cvtColor(img, img, COLOR_RGB2BGR);

	try {
		dest = gcapp.bgRemove(img, top, left, width,height);
		
		
	}
	catch (Exception ex)
	{
		return NULL;
	}
	

	int params[3] = { 0 };
	params[0] = IMWRITE_JPEG_QUALITY;
	params[1] = 100;

	
	vector<uchar> buf;
	bool code = cv::imencode(".jpg", dest, buf, std::vector<int>(params, params + 2));
	uchar* result = reinterpret_cast<uchar*> (&buf[0]);

	string strretbase64 =base64_encode(result, buf.size());

	//waitKey();

	char* pcharRet = new char[strretbase64.length()];
	strcpy(pcharRet, strretbase64.c_str());
	return pcharRet;

	
}







//
//
//static const std::string base64_chars =
//"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
//"abcdefghijklmnopqrstuvwxyz"
//"0123456789+/";
//
//
//static inline bool is_base64(unsigned char c) {
//	return (isalnum(c) || (c == '+') || (c == '/'));
//}
//
//std::string base64_decode(std::string const& encoded_string) {
//	int in_len = encoded_string.size();
//	int i = 0;
//	int j = 0;
//	int in_ = 0;
//	unsigned char char_array_4[4], char_array_3[3];
//	std::string ret;
//
//	while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
//		char_array_4[i++] = encoded_string[in_]; in_++;
//		if (i == 4) {
//			for (i = 0; i < 4; i++)
//				char_array_4[i] = base64_chars.find(char_array_4[i]);
//
//			char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
//			char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
//			char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
//
//			for (i = 0; (i < 3); i++)
//				ret += char_array_3[i];
//			i = 0;
//		}
//	}
//
//	if (i) {
//		for (j = i; j < 4; j++)
//			char_array_4[j] = 0;
//
//		for (j = 0; j < 4; j++)
//			char_array_4[j] = base64_chars.find(char_array_4[j]);
//
//		char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
//		char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
//		char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
//
//		for (j = 0; (j < i - 1); j++) ret += char_array_3[j];
//	}
//
//	return ret;
//}
