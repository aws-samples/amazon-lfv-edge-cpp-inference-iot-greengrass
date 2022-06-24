/*
 * @main.cc
 * @brief Running Inference using LFV Edge Agent
 * 
 * This main app would take in input and LFV model name. 
 * It would use gRPC to connect to the LFV unix socket. 
 * Using OpenCV, the input data would be converted to right format and passed to the gRPC socket for running inference.
 * Once inference is success, the output is then printed.
 * 
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: MIT-0
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <iostream>
#include <memory>
#include <queue>
#include <thread>
#include <chrono>
#include <mutex>
#include <string>
#include <stdint.h>
#include <dirent.h>
#include <grpcpp/grpcpp.h>
#include "edge-agent.grpc.pb.h"
#include <opencv2/opencv.hpp>

const char *LfveModelStatusE[] = {"STOPPED", "STARTING", "RUNNING", "FAILED", "STOPPING"};

/**
    Getting file extensions
    @param s filename as a string
    @return filename extension as a string
*/
std::string getFileExt(std::string& s)
{
    size_t i = s.rfind('.', s.length());
    if (i != std::string::npos) {
        return(s.substr(i+1, s.length() - i));
    }
    return("");
}

int main(int argc, char *argv[])
{
    std::cout << "[LFVE EdgeAgent C++] LFV Inference GG Component Started" << std::endl;

    std::string inputFile_ = argv[1];
    std::string modelName_ = argv[2];
    std::string ext = getFileExt(inputFile_);
    int totalFrames = -100, height = -1, width = -1;

    cv::Mat bgrImage, rgbImage;
    cv::VideoCapture cap;
    
    if (ext.compare("jpg")==0 || ext.compare("JPG")==0 || ext.compare("png")==0 || ext.compare("PNG")==0 || ext.compare("jpeg")==0 || ext.compare("JPEG")==0)
    {
        std::cout << "[LFVE EdgeAgent C++] Image Location = " << inputFile_ << std::endl;
        bgrImage = cv::imread(inputFile_);
        if (bgrImage.empty())
        {
            std::cerr << "[LFVE EdgeAgent C++] ERROR! Unable to find image file" << std::endl;
            return -1;
        }
        cv::cvtColor(bgrImage, rgbImage, cv::COLOR_BGR2RGB);
        height = bgrImage.size().height;
        width = bgrImage.size().width;
        std::cout << "[LFVE EdgeAgent C++] Image Size (HxW) = (" << height << ","<< width << ")" << std::endl;
    }
    else if (std::isdigit(std::atoi(inputFile_.c_str())))
    {
        std::cout << "[LFVE EdgeAgent C++] Camera ID = " << inputFile_ << std::endl;
        cap.open(std::atoi(inputFile_.c_str()));
        if (!cap.isOpened())
        {
            std::cerr << "[LFVE EdgeAgent C++] ERROR! Unable to open camera" << std::endl;
            return -1;
        }
        totalFrames = -1;
        height = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
        width = cap.get(cv::CAP_PROP_FRAME_WIDTH);
        std::cout << "[LFVE EdgeAgent C++] Camera Size (HxW) = (" << height << ","<< width << ")" << std::endl;
    }
    else if (ext.compare("avi")==0 || ext.compare("mp4")==0 || ext.compare("flv")==0 || ext.compare("AVI")==0 || ext.compare("MP4")==0 || ext.compare("FLV")==0)
    {
        std::cout << "[LFVE EdgeAgent C++] Video Location = " << inputFile_ << std::endl;
        cv::VideoCapture cap;
        cap.open(inputFile_);
        if (!cap.isOpened())
        {
            std::cerr << "[LFVE EdgeAgent C++] ERROR! Unable to find video file" << std::endl;
            return -1;
        }
        totalFrames = cap.get(cv::CAP_PROP_FRAME_COUNT);
        height = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
        width = cap.get(cv::CAP_PROP_FRAME_WIDTH);
        std::cout << "[LFVE EdgeAgent C++] Video Size (HxW) = (" << height << ","<< width << ")" << std::endl;
    }
    else
    {
        std::cerr << "[LFVE EdgeAgent C++] ERROR! Unable to find image/video file or open camera" << std::endl;
        return -1;
    }

    std::shared_ptr< grpc::Channel> channel = grpc::CreateChannel("unix:///tmp/aws.iot.lookoutvision.EdgeAgent.sock", grpc::InsecureChannelCredentials());
    std::unique_ptr< AWS::LookoutVision::EdgeAgent::Stub> stubs = AWS::LookoutVision::EdgeAgent::NewStub(channel); // inference stubs for LFV model
    
    bool is_anomaly_overall;
    float confidence_overall;
    grpc::ClientContext context; // single context will be used for LFV models
    AWS::LookoutVision::DetectAnomaliesRequest request; // single request will be used for LFV models
    AWS::LookoutVision::DetectAnomaliesResponse response; // single response will be used for LFV models
    AWS::LookoutVision::DetectAnomalyResult reply; // single reply will be used for LFV models
    AWS::LookoutVision::Bitmap bitmap; // bitmap image creation
    AWS::LookoutVision::DescribeModelRequest dmRequest;
    AWS::LookoutVision::DescribeModelResponse dmResponse;
    AWS::LookoutVision::ModelDescription modelDescription;
    AWS::LookoutVision::Bitmap anomaly_mask; // output bitmap mask
    AWS::LookoutVision::Anomaly anomalies; // output anomalies
    std::vector<AWS::LookoutVision::Anomaly> anomaliesVec;
    AWS::LookoutVision::Bitmap* lfveBMret;

    request.set_model_component(modelName_);
    dmRequest.set_model_component(modelName_);
    stubs->DescribeModel(&context, dmRequest, &dmResponse);
    modelDescription = dmResponse.model_description();

    // Check for LFV Model Status: STARTING/STOPPED/STOPPING/FAILED
    // If the Model Status is STOPPED, try to START it for sometime until returning FAIL
    if (modelDescription.status() == AWS::LookoutVision::STOPPED || modelDescription.status() == AWS::LookoutVision::STOPPING || modelDescription.status() == AWS::LookoutVision::STARTING) // model is STOPPED -> START
    {
        if (modelDescription.status() == AWS::LookoutVision::STOPPED || modelDescription.status() == AWS::LookoutVision::STOPPING)
        {
            grpc::ClientContext context;
            std::cout << "[LFVE EdgeAgent C++] model is STOPPED. Starting now..." << std::endl;
            AWS::LookoutVision::StartModelRequest smRequest; smRequest.set_model_component(modelName_);
            AWS::LookoutVision::StartModelResponse smResponse;
            stubs->StartModel(&context, smRequest, &smResponse);
        }
        grpc::ClientContext context;
        dmRequest.set_model_component(modelName_);
        stubs->DescribeModel(&context, dmRequest, &dmResponse);
        modelDescription = dmResponse.model_description();
        std::cout << "[LFVE EdgeAgent C++] model is starting with status: " << LfveModelStatusE[modelDescription.status()] << std::endl;
        
        // Check until model is running for 10 iterations each of 30 seconds
        int check_iter = 0, max_check_iter = 10;
        bool model_isRunning = false;
        while (check_iter<max_check_iter)
        {
            check_iter++;
            grpc::ClientContext context;
            dmRequest.set_model_component(modelName_);
            stubs->DescribeModel(&context, dmRequest, &dmResponse);
            modelDescription = dmResponse.model_description();
            std::cout << "[LFVE EdgeAgent C++] model is starting with status: " << LfveModelStatusE[modelDescription.status()] << " (" << check_iter << "/" << max_check_iter << ")" << std::endl;
            if (modelDescription.status() == AWS::LookoutVision::RUNNING)
            {
                std::cout << "[LFVE EdgeAgent C++] model has status: " << LfveModelStatusE[modelDescription.status()] << std::endl;
                model_isRunning = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5000)); // Wait for 5 seconds before checking again
        }
    }
    else if (modelDescription.status() == AWS::LookoutVision::RUNNING)
    {
        std::cout << "[LFVE EdgeAgent C++] model has status: " << LfveModelStatusE[modelDescription.status()] << std::endl;
    }
    else if (modelDescription.status() == AWS::LookoutVision::FAILED)
    {
        std::cout << "[LFVE EdgeAgent C++] model has status: " << LfveModelStatusE[modelDescription.status()] << std::endl;
    }

    int iter = 0;
    while(1==1)
    {
        iter++;

        if (totalFrames>=-1)
        {
            cap.read(bgrImage);
            cv::cvtColor(bgrImage, rgbImage, cv::COLOR_BGR2RGB);
        }

        if (rgbImage.empty()) 
        {
            std::cerr << "[LFVE EdgeAgent C++] ERROR! blank frame grabbed" << std::endl;
            break;
        }

        bitmap.set_width(width);
        bitmap.set_height(height);
        bitmap.set_byte_data(rgbImage.data, rgbImage.total() * rgbImage.elemSize());
        request.set_allocated_bitmap(&bitmap);
        grpc::Status status = stubs->DetectAnomalies(&context, request, &response);
        reply = response.detect_anomaly_result();
        is_anomaly_overall = reply.is_anomalous(); // true:anomaly, false:normal
        confidence_overall = reply.confidence(); // confidence 
        anomaly_mask = reply.anomaly_mask(); // output anomaly mask
        anomaliesVec = std::vector<AWS::LookoutVision::Anomaly>(reply.anomalies().begin(), reply.anomalies().end()); // output anomalies
        lfveBMret = request.release_bitmap();

        // Print the Inference Details
        std::cout << "[LFVE EdgeAgent C++] Inference Results:" << std::endl;
        std::cout << "    is_anomalous: " << is_anomaly_overall << std::endl;
        std::cout << "    confidence: " << confidence_overall << std::endl;
        for (int avec=0; avec<anomaliesVec.size(); avec++)
        {
            std::cout << "    Anomalies #" << avec+1 << std::endl;
            std::cout << "        ---------------------" << std::endl;
            std::cout << "        Total Percentage Area: " << std::to_string(anomaliesVec[avec].pixel_anomaly().total_percentage_area()) << std::endl;
            std::cout << "        Hex Color: " << anomaliesVec[avec].pixel_anomaly().hex_color() << std::endl;
            std::cout << "        Name: " << anomaliesVec[avec].name() << std::endl;
            std::cout << "        ---------------------" << std::endl;
        }
        std::cout << "    Anomaly Mask Size (HxW): (" << (int)anomaly_mask.height() << "," << (int)anomaly_mask.width() << ")" << std::endl;
        
        // Ending Program if Video ends or if ESCAPE is pressed
        char key = cv::waitKey(1);
        if ((iter>=totalFrames && totalFrames!=-1) || key == 27)
        {
            std::cout << "End of Program" << std::endl;
            break;
        }
    }

    return 0;
}