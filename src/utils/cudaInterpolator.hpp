#include <iostream>
#include <iomanip>
#include <string>

#include "opencv2/core/core.hpp"
#include <opencv2/core/cuda.hpp>
#include "opencv2/highgui/highgui.hpp"
#include <opencv2/imgproc/imgproc.hpp> 
#include <opencv2/cudaoptflow.hpp>
#include <opencv2/cudaarithm.hpp>
#include <opencv2/cudalegacy.hpp>
//#include "opencv2/gpu/gpu.hpp"

using namespace std;
using namespace cv;
using namespace cv::cuda;

template <typename T> inline T clamp (T x, T a, T b)
{
    return ((x) > (a) ? ((x) < (b) ? (x) : (b)) : (a));
}

template <typename T> inline T mapValue(T x, T a, T b, T c, T d)
{
    x = clamp(x, a, b);
    return c + (d - c) * (x - a) / (b - a);
}

void getFlowField(const Mat flow, Mat& flowField)
{
    Mat uv[2];
    cv::split(flow, uv);

    float maxDisplacement = 1.0f;

    for (int i = 0; i < uv[0].rows; ++i)
    {
        const float* ptr_u = uv[0].ptr<float>(i);
        const float* ptr_v = uv[1].ptr<float>(i);

        for (int j = 0; j < uv[0].cols; ++j)
        {
            float d = max(fabsf(ptr_u[j]), fabsf(ptr_v[j]));

            if (d > maxDisplacement)
                maxDisplacement = d;
        }
    }

    flowField.create(uv[0].size(), CV_8UC4);

    for (int i = 0; i < flowField.rows; ++i)
    {
        const float* ptr_u = uv[0].ptr<float>(i);
        const float* ptr_v = uv[1].ptr<float>(i);


        Vec4b* row = flowField.ptr<Vec4b>(i);

        for (int j = 0; j < flowField.cols; ++j)
        {
            row[j][0] = 0;
            row[j][1] = static_cast<unsigned char> (mapValue (-ptr_v[j], -maxDisplacement, maxDisplacement, 0.0f, 255.0f));
            row[j][2] = static_cast<unsigned char> (mapValue ( ptr_u[j], -maxDisplacement, maxDisplacement, 0.0f, 255.0f));
            row[j][3] = 255;
        }
    }
}

std::vector<Mat> Interpolate(Mat &frame0, Mat &frame1, float step = 0.5)
{
	std::vector<Mat> out;
	float scale = 0.8f;
	float alpha = 0.197f;
	float gamma = 50.0f;
	int inner_iterations = 10;
	int outer_iterations = 77;
	int solver_iterations = 10;
	
	Mat frame0float;
	Mat frame1float;
	frame0.convertTo(frame0float, CV_32F, 1.0 / 255.0);
	frame1.convertTo(frame1float, CV_32F, 1.0 / 255.0);

	Mat frame0gray, frame1gray;
	cv::cvtColor(frame0float, frame0gray, COLOR_BGR2GRAY);
	cv::cvtColor(frame1float, frame1gray, COLOR_BGR2GRAY);

	cv::cuda::GpuMat d_frame0;
	cv::cuda::GpuMat d_frame1;
	d_frame0.upload(frame0gray);
	d_frame1.upload(frame1gray);

	Ptr<BroxOpticalFlow> d_flow = BroxOpticalFlow::create(alpha, gamma, scale, inner_iterations, outer_iterations, solver_iterations);

	GpuMat d_forward;
	d_flow->calc(d_frame0, d_frame1, d_forward);
	//Mat flowFieldForward;
	//getFlowField(d_forward, flowFieldForward);
	
	GpuMat d_backward;
	d_flow->calc(d_frame1, d_frame0, d_backward);
	//Mat flowFieldBackward;
	//getFlowField(d_backward, flowFieldBackward);


	// first frame color components
	GpuMat d_b, d_g, d_r;

	// second frame color components
	GpuMat d_bt, d_gt, d_rt;

	// prepare color components on host and copy them to device memory
	Mat channels[3];
	cv::split(frame0float, channels);

	d_b.upload(channels[0]);
	d_g.upload(channels[1]);
	d_r.upload(channels[2]);

	cv::split(frame1float, channels);

	d_bt.upload(channels[0]);
	d_gt.upload(channels[1]);
	d_rt.upload(channels[2]);

	GpuMat d_buf;
	GpuMat d_rNew, d_gNew, d_bNew;
	GpuMat d_newFrame;

	GpuMat d_f_uv[2];
	cv::cuda::split(d_forward, d_f_uv);
	GpuMat d_b_uv[2];
	cv::cuda::split(d_backward, d_b_uv);

	for(float a = step; a < 1.0f; a += step)
	{
		// interpolate blue channel
		cuda::interpolateFrames(d_b, d_bt, d_f_uv[0], d_f_uv[1], d_b_uv[0], d_b_uv[1], a, d_bNew, d_buf);

		// interpolate green channel
		cuda::interpolateFrames(d_g, d_gt, d_f_uv[0], d_f_uv[1], d_b_uv[0], d_b_uv[1], a, d_gNew, d_buf);

		// interpolate red channel
		cuda::interpolateFrames(d_r, d_rt, d_f_uv[0], d_f_uv[1], d_b_uv[0], d_b_uv[1], a, d_rNew, d_buf);

		GpuMat channels3[] = {d_bNew, d_gNew, d_rNew};
		cv::cuda::merge(channels3, 3, d_newFrame);

		out.push_back(Mat(d_newFrame));
	}	

	return out;
}

