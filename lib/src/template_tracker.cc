#include "dove_eye/template_tracker.h"

#include <opencv2/opencv.hpp>

#include "config.h"
#include "dove_eye/cv_logging.h"
#include "dove_eye/logging.h"

using cv::matchTemplate;
using cv::meanStdDev;
using cv::minMaxLoc;

namespace dove_eye {

bool TemplateTracker::InitTrackerData(const cv::Mat &data, const Mark &mark) {
  assert(mark.type == Mark::kCircle);
  DEBUG("%p->%s(data, %f@[%f,%f])", this, __func__,
        mark.radius,
        mark.center.x, mark.center.y);

  const auto point = mark.center;
  const auto radius = mark.radius;

  if (point.x < radius || point.x >= data.cols - radius ||
      point.y < radius || point.y >= data.rows - radius) {
    return false;
  }

  cv::Rect roi(point.x - radius, point.y - radius, 2 * radius, 2 * radius);

  /* We don't want to have the template overwritten */
  data_.search_template = data(roi).clone();
  data_.radius = radius;

  return true;
}

/** Wrapper for OpenCV function matchTemplate
 * @see SearchingTracker::Search()
 */
bool TemplateTracker::Search(
      const cv::Mat &data,
      TrackerData &tracker_data,
      const cv::Rect *roi,
      const cv::Mat *mask,
      const double threshold,
      Mark *result) const {
  const TemplateData &tpl = static_cast<const TemplateData &>(tracker_data);

  DEBUG("%p->%s([%i, %i], %f, %p[%i, %i]@[%i, %i], %p, %f, res)",
        this, __func__,
        data.cols, data.rows,
        tpl.radius,
        roi, (roi ? roi->width : 0), (roi ? roi->height : 0),
             (roi ? roi->x : 0),     (roi ? roi->y : 0),
        mask,
        threshold);


  auto extended_roi = cv::Rect(cv::Point(0, 0), data.size());
  if (roi) {
    extended_roi = cv::Rect(tpl.TopLeft(roi->tl()), tpl.BottomRight(roi->br()));
    extended_roi &= cv::Rect(cv::Point(0, 0), data.size());
  }

  if (extended_roi.width < tpl.search_template.cols ||
      extended_roi.height < tpl.search_template.rows) {
    DEBUG("%p->%s small-roi", this, __func__);
    return false;
  }

  /* Experimentally CV_TM_CCOEFF_NORMED gave best results */
  //const int method = CV_TM_SQDIFF_NORMED;
  //const int method = CV_TM_CCORR_NORMED
  const int method = CV_TM_CCOEFF_NORMED;

  cv::Mat match_result;
  matchTemplate(data(extended_roi), tpl.search_template, match_result, method);


  double min_val;
  cv::Point min_loc;
  double max_val;
  cv::Point max_loc;
  cv::Scalar std_dev;

  cv::Mat shifted_mask;
  if (mask) {
    /* Mask is first cropped with same ROI as image */
    auto cropped_mask = (*mask)(extended_roi);

    /* Consequently mask is shifted, which is effectively cropping top-left
     * corner */
    cv::Point mask_br(cropped_mask.cols, cropped_mask.rows);
    cv::Rect shift_rect(-tpl.TopLeft(), mask_br - tpl.BottomRight());

    /* We've worked with (zero-based) coordinates, add one to size */
    shift_rect.width += 1;
    shift_rect.height += 1;

    shifted_mask = cropped_mask(shift_rect);

    assert(match_result.rows == shifted_mask.rows);
    assert(match_result.cols == shifted_mask.cols);

    minMaxLoc(match_result, &min_val, &max_val, &min_loc, &max_loc, shifted_mask);
    meanStdDev(match_result, cv::noArray(), std_dev, shifted_mask);
  } else {
    minMaxLoc(match_result, &min_val, &max_val, &min_loc, &max_loc);
    meanStdDev(match_result, cv::noArray(), std_dev);
  }

  const double value = (method == CV_TM_SQDIFF_NORMED) ? (1-min_val) :
      (method == CV_TM_CCORR_NORMED) ? max_val :
      (method == CV_TM_CCOEFF_NORMED) ? (max_val - min_val) : 0;

#ifdef CONFIG_DEBUG_HIGHGUI
  cv::Mat to_show;
  if (mask) {
    cv::Mat masked;
    match_result.copyTo(masked, shifted_mask);

    to_show = (masked - min_val) / value;
    log_mat((reinterpret_cast<size_t>(this) * 100) + 10, to_show);
  } else {
    to_show = (match_result - min_val) / value;
    log_mat((reinterpret_cast<size_t>(this) * 100) + 10, to_show);
  }
#endif


  if (value <= threshold) {
#ifdef CONFIG_DEBUG_HIGHGUI
    log_mat(reinterpret_cast<size_t>(this) * 100 + 1, data(extended_roi).clone());
    log_mat(reinterpret_cast<size_t>(this) * 100 + 2, tpl.search_template);
    log_mat(reinterpret_cast<size_t>(this) * 100 + 2, to_show.clone());
#endif
    DEBUG("%p->%s low value (%f/%f)", this, __func__, value, threshold);
    return false;
  }

  // TODO return false also when minumum is shallow (i.e. not unique match)

  const auto loc = (method == CV_TM_SQDIFF_NORMED) ? min_loc :
      (method == CV_TM_CCORR_NORMED) ? max_loc :
      (method == CV_TM_CCOEFF_NORMED) ? (max_loc) : cv::Point();

  /* Transform coordinates of found matchpoint to whole image */
  cv::Point tpl_offset = -tpl.TopLeft(cv::Point(0, 0));
  auto match_point = Point2(loc.x, loc.y) + Point2(tpl_offset.x, tpl_offset.y);
  match_point += Point2(extended_roi.x, extended_roi.y);

  result->type = Mark::kCircle;
  result->center = match_point;
  result->radius = tpl.radius;
  
  DEBUG("%p->%s matched (%f/%f)", this, __func__, value, threshold);
  return true;
}


} // namespace dove_eye
