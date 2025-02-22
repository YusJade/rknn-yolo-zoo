#include "yolo11.h"

#include <cassert>
#include <chrono>
#include <thread>
#include <vector>

#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <spdlog/spdlog.h>

#include "postprocess.h"

using namespace rknn_yolo_inference;

Yolov7::Yolov7(std::string model) : IYolo() {
  int ret;
  memset(&rknn_app_ctx_, 0, sizeof(rknn_app_context_t));

  init_post_process();

  ret = init_yolo11_model(model.c_str(), &rknn_app_ctx_);
  if (ret != 0) {
    spdlog::error("init_yolo11_model fail! ret={} model_path={}\n", ret,
                  model.c_str());
    assert(ret == 0);
  }
}

DetectResult Yolov7::Detect(cv::Mat img0) {
  image_buffer_t src_image;
  memset(&src_image, 0, sizeof(image_buffer_t));

  cv::Mat det_img = img0.clone();
  cv::cvtColor(det_img, det_img, cv::COLOR_BGR2RGB);

  src_image.format = IMAGE_FORMAT_RGB888;
  src_image.width = det_img.cols;
  src_image.height = det_img.rows;
  src_image.width_stride = det_img.step;
  src_image.height_stride = det_img.rows;
  src_image.virt_addr = det_img.data;
  src_image.size = det_img.total() * det_img.elemSize();
  src_image.fd = -1;

  std::vector<DetectItem> result;

  object_detect_result_list od_results;
  // od_results.count = 0;
  int ret = inference_yolo11_model(&rknn_app_ctx_, &src_image, &od_results);
  for (int i = 0; i < od_results.count; i++) {
    object_detect_result* det_result = &(od_results.results[i]);
    int x1 = det_result->box.left;
    int y1 = det_result->box.top;
    int x2 = det_result->box.right;
    int y2 = det_result->box.bottom;

    result.push_back({cv::Rect2f(x1, y1, x2 - x1, y2 - y1), det_result->cls_id,
                      coco_cls_to_name(det_result->cls_id), det_result->prop});

    cv::rectangle(det_img, cv::Rect(x1, y1, x2 - x1, y2 - y1),
                  cv::Scalar(0, 255, 0));
    cv::putText(det_img, coco_cls_to_name(det_result->cls_id),
                cv::Point(x1, y1 - 3), cv::FONT_HERSHEY_SIMPLEX, 0.5,
                cv::Scalar(0, 255, 0));
    spdlog::debug("detected box: {} @ ({} {} {} {}) {}",
                  coco_cls_to_name(det_result->cls_id), det_result->box.left,
                  det_result->box.top, det_result->box.right,
                  det_result->box.bottom, det_result->prop);
    // draw_rectangle(&src_image, x1, y1, x2 - x1, y2 - y1, COLOR_BLUE, 3);

    // sprintf(text, "%s %.1f%%", coco_cls_to_name(det_result->cls_id),
    //         det_result->prop * 100);
    // draw_text(&src_image, text, x1, y1 - 20, COLOR_RED, 10);
  }

  cv::Mat frame;
  cv::cvtColor(det_img, frame, cv::COLOR_RGB2BGR);
  Notify(frame);
  // deinit_post_process();

  // ret = release_yolo11_model(&rknn_app_ctx);
  // if (ret != 0) {
  //   printf("release_yolo11_model fail! ret=%d\n", ret);
  // }

  // if (src_image.virt_addr != NULL) {
  //   free(src_image.virt_addr);
  // }

  return {result, frame};
}

bool Yolov7::Save() {}