#include "caffe_encoder_utils.h"

#include <iostream>

#include <glog/logging.h>

//#define DEBUG_CAFFE_CHECKSUM

#ifdef DEBUG_CAFFE_CHECKSUM
#include <boost/crc.hpp>
#endif

#define IMAGE_DIM 256
#define CROPPED_DIM 224

#define CH_COUNT 3

namespace featpipe {

  /// Load the mean image from a Matlab-generated MAT file
  cv::Mat loadMeanImageFile(const std::string& mean_image_file) {

    caffe::Blob<float> data_mean;
    LOG(INFO) << "Loading mean file from" << mean_image_file;
    caffe::BlobProto blob_proto;
    bool result = caffe::ReadProtoFromBinaryFile(mean_image_file.c_str(), &blob_proto);
    if (!result) {
      LOG(FATAL) << "Couldn't read the file: " << mean_image_file;
    }
    data_mean.FromProto(blob_proto);
    DLOG(INFO) << ("Remember that Caffe saves in [width, height, channels]"
                   " format and channels are also BGR!");

    CHECK_EQ(data_mean.channels(), 3);
    cv::Mat mean_image = cv::Mat::zeros(data_mean.height(), data_mean.width(), CV_32FC3);

    caffe::caffe_copy(data_mean.count(), data_mean.cpu_data(),
                      (float*)mean_image.data);
    return mean_image;
  }

  /// Downsize image to MIN_SIZE x N where MIN_SIZE is the smaller dimension
  cv::Mat downsizeToBound(const cv::Mat& src, const size_t min_size) {

    cv::Mat dst;
    float sf = 1.0;

    if (src.rows > src.cols) {
      sf = static_cast<float>(min_size)/static_cast<float>(src.cols);
    } else {
      sf = static_cast<float>(min_size)/static_cast<float>(src.rows);
    }
    DLOG(INFO) << "Downsize scale factor is: " << sf;
    cv::resize(src, dst, cv::Size(), sf, sf);

    return dst;
  }

  /// Function for loading and preparing an image for use in prepareCaffeImages
  cv::Mat getBaseCaffeImage(const cv::Mat& src, const size_t min_size) {
    return downsizeToBound(src, min_size);
  }

  /// Function for getting whole image crop of size IMAGE_DIM x IMAGE_DIM for use in prepareCaffeImage
  cv::Mat getWholeCropCaffeImage(const cv::Mat& src, const size_t crop_size) {

    cv::Mat dst = getBaseCaffeImage(src, crop_size);

    cv::Mat cropped_dst;
    size_t new_width = dst.cols;
    size_t new_height = dst.rows;
    if (new_height > new_width) {
      size_t excess_sz = (new_height - new_width) / 2;
      dst(cv::Rect(0, excess_sz, crop_size, crop_size)).copyTo(cropped_dst);
    } else if (new_width > new_height) {
      size_t excess_sz = (new_width - new_height) / 2;
      dst(cv::Rect(excess_sz, 0, crop_size, crop_size)).copyTo(cropped_dst);
    } else {
      dst.copyTo(cropped_dst);
    }

    return cropped_dst;
  }

  /// Check dimensions of images against blob
  size_t checkImagesAgainstBlob(const std::vector<cv::Mat>& images,
                                const caffe::Blob<float>* blob) {

    CHECK_EQ(blob->num(), images.size());
    CHECK_EQ(blob->channels(), CH_COUNT);
    for (size_t i = 0; i < images.size(); ++i) {
      CHECK_EQ(blob->height(), images[i].rows);
      CHECK_EQ(blob->width(), images[i].cols);
    }
    CHECK_EQ(blob->count(), blob->width()*blob->height()*blob->channels()*blob->num());

    return images[0].rows*images[0].cols;
  }

  /// Add test images to a Caffe net
  void setNetTestImages(const std::vector<cv::Mat>& images, caffe::Net<float>& net) {
    std::vector<caffe::Blob<float>*>& input_blobs = net.input_blobs();
    CHECK_EQ(input_blobs.size(), 1);

    DLOG(INFO) << "Input blob info:" << std::endl
               << "  num(): " << input_blobs[0]->num() << std::endl
               << "  channels(): " << input_blobs[0]->channels() << std::endl
               << "  height(): " << input_blobs[0]->height() << std::endl
               << "  width(): " << input_blobs[0]->width() << std::endl
               << "  count(): " << input_blobs[0]->count() << std::endl;

    checkImagesAgainstBlob(images, input_blobs[0]);

    size_t im_pix_sz = input_blobs[0]->count() / input_blobs[0]->num();

    for (size_t im_idx = 0; im_idx < images.size(); ++im_idx) {

      CHECK_EQ(images[im_idx].depth(), CV_32F);
      CHECK_EQ(images[im_idx].channels(), 3);
      CHECK_EQ(im_pix_sz,
               images[im_idx].rows*images[im_idx].cols*images[im_idx].channels());

      const float* data_ptr = (float*)images[im_idx].data;

      caffe::caffe_copy(im_pix_sz, data_ptr,
                        input_blobs[0]->mutable_cpu_data() + im_pix_sz*im_idx);
    }

  }

}
