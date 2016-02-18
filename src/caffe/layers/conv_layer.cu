#include <vector>

#include "caffe/layers/conv_layer.hpp"

#ifdef USE_GREENTEA
#include "caffe/greentea/greentea.hpp"
#include "caffe/greentea/greentea_im2col.hpp"
#include "caffe/greentea/greentea_math_functions.hpp"
#endif

#include <chrono>

namespace caffe {

template <typename Dtype>
void ConvolutionLayer<Dtype>::Forward_gpu(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {

  std::chrono::time_point<std::chrono::system_clock> start, end;
  start = std::chrono::system_clock::now();
  std::chrono::time_point<std::chrono::system_clock> startx, endx;

  const Dtype* weight = this->blobs_[0]->gpu_data();
  for (int_tp i = 0; i < bottom.size(); ++i) {
    const Dtype* bottom_data = bottom[i]->gpu_data();
    Dtype* top_data = top[i]->mutable_gpu_data();
    // Multi queue execution, all previous work needs to be done first

    startx = std::chrono::system_clock::now();
    // this->device_->FinishQueues();
    endx = std::chrono::system_clock::now();
    VLOG(1) << "queue1 " << std::chrono::duration<double, std::milli>(endx - startx).count();

    for (int_tp n = 0; n < this->num_; ++n) {
      // Multi queue execution, go through work queues
      startx = std::chrono::system_clock::now();
      this->device_->SwitchQueue(n);
      endx = std::chrono::system_clock::now();
      VLOG(1) << "switch " << std::chrono::duration<double, std::milli>(endx - startx).count();

      
      startx = std::chrono::system_clock::now();
      this->forward_gpu_gemm(bottom_data, n * this->bottom_dim_, weight,
          top_data,  n * this->top_dim_);
      endx = std::chrono::system_clock::now();
      VLOG(1) << "prod " << std::chrono::duration<double, std::milli>(endx - startx).count();
      if (this->bias_term_) {
        startx = std::chrono::system_clock::now();
        const Dtype* bias = this->blobs_[1]->gpu_data();
        this->forward_gpu_bias(top_data, n * this->top_dim_, bias);
        // this->device_->FinishQueues(); // TODO: temp.
        endx = std::chrono::system_clock::now();
        VLOG(1) << "bias " << std::chrono::duration<double, std::milli>(endx - startx).count();
      }
    }
    // Multi queue execution, finish all queues
    startx = std::chrono::system_clock::now();
    endx = std::chrono::system_clock::now();
    VLOG(1) << "queue2 " << std::chrono::duration<double, std::milli>(endx - startx).count();
  }

  this->device_->FinishQueues();
  end = std::chrono::system_clock::now();
  std::chrono::duration<double, std::milli> fp_ms = end - start;

  VLOG(1) << "conv " << fp_ms.count();
}

template <typename Dtype>
void ConvolutionLayer<Dtype>::Backward_gpu(const vector<Blob<Dtype>*>& top,
      const vector<bool>& propagate_down, const vector<Blob<Dtype>*>& bottom) {
  const Dtype* weight = this->blobs_[0]->gpu_data();
  Dtype* weight_diff = this->blobs_[0]->mutable_gpu_diff();
  for (int_tp i = 0; i < top.size(); ++i) {
    const Dtype* top_diff = top[i]->gpu_diff();
    // Bias gradient, if necessary.
    if (this->bias_term_ && this->param_propagate_down_[1]) {
      Dtype* bias_diff = this->blobs_[1]->mutable_gpu_diff();
      for (int_tp n = 0; n < this->num_; ++n) {
        this->backward_gpu_bias(bias_diff, top_diff, n * this->top_dim_);
      }
    }
    if (this->param_propagate_down_[0] || propagate_down[i]) {
      const Dtype* bottom_data = bottom[i]->gpu_data();
      Dtype* bottom_diff = bottom[i]->mutable_gpu_diff();
      for (int_tp n = 0; n < this->num_; ++n) {
        // gradient w.r.t. weight. Note that we will accumulate diffs.
        if (this->param_propagate_down_[0]) {
          this->weight_gpu_gemm(bottom_data, n * this->bottom_dim_,
              top_diff, n * this->top_dim_, weight_diff);
        }
      }
      // gradient w.r.t. bottom data, if necessary.
      if (propagate_down[i]) {
        // Multi queue execution, all previous work needs to be done first
        this->device_->FinishQueues();
        for (int_tp n = 0; n < this->num_; ++n) {
          // Multi queue execution, go through work queues
          this->device_->SwitchQueue(n);
          this->backward_gpu_gemm(top_diff, n * this->top_dim_, weight,
                                  bottom_diff, n * this->bottom_dim_);
        }
        // Multi queue execution, finish all queues
        this->device_->FinishQueues();
      }
    }
  }
}

INSTANTIATE_LAYER_GPU_FUNCS(ConvolutionLayer);

}  // namespace caffe
