#include <string.h>
#include <jni.h>
#include <android/log.h>
#include <string>
#include <vector>

#ifdef USE_EIGEN
#include <omp.h>
#else
#include <cblas.h>
#endif

#include "caffe/caffe.hpp"
#include "caffe_mobile.hpp"

#ifdef __cplusplus
extern "C" {
#endif


//// GPU Profiling.
//#include <android/log.h>
//#include <dlfcn.h>
//
//#define PACKAGE_NAME "com.happen.it.make.whatsthis" // Fill this in with the actual package name
//#define GAPII_SO_PATH "/data/data/" PACKAGE_NAME "/lib/libgapii.so"
//
//struct GapiiLoader {
//    GapiiLoader() {
//        if (!dlopen(GAPII_SO_PATH, RTLD_LOCAL | RTLD_NOW)) {
//            __android_log_print(ANDROID_LOG_ERROR, "GAPII", "Failed loading " GAPII_SO_PATH);
//        }
//    }
//};
//
//GapiiLoader __attribute__((used)) gGapiiLoader;



using std::string;
using std::vector;
using caffe::CaffeMobile;

int getTimeSec() {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (int)now.tv_sec;
}

string jstring2string(JNIEnv *env, jstring jstr) {
  const char *cstr = env->GetStringUTFChars(jstr, 0);
  string str(cstr);
  env->ReleaseStringUTFChars(jstr, cstr);
  return str;
}

JNIEXPORT void JNICALL
Java_strin_caffe_CaffeMobileOpenCL_setNumThreads(
    JNIEnv *env, jobject thiz, jint numThreads) {
  int num_threads = numThreads;
#ifdef USE_EIGEN
  omp_set_num_threads(num_threads);
#else
  openblas_set_num_threads(num_threads);
#endif
}

JNIEXPORT void JNICALL
Java_strin_caffe_CaffeMobileOpenCL_enableLog(JNIEnv *env,
                                                         jobject thiz,
                                                         jboolean enabled) {}

JNIEXPORT jint JNICALL Java_strin_caffe_CaffeMobileOpenCL_loadModel(
    JNIEnv *env, jobject thiz, jstring modelPath, jstring weightsPath) {
  CaffeMobile::Get(jstring2string(env, modelPath),
                   jstring2string(env, weightsPath));
  return 0;
}

JNIEXPORT void JNICALL
Java_strin_caffe_CaffeMobileOpenCL_setMeanWithMeanFile(
    JNIEnv *env, jobject thiz, jstring meanFile) {
  CaffeMobile *caffe_mobile = CaffeMobile::Get();
  caffe_mobile->SetMean(jstring2string(env, meanFile));
}

JNIEXPORT void JNICALL
Java_strin_caffe_CaffeMobileOpenCL_setMeanWithMeanValues(
    JNIEnv *env, jobject thiz, jfloatArray meanValues) {
  CaffeMobile *caffe_mobile = CaffeMobile::Get();
  int num_channels = env->GetArrayLength(meanValues);
  jfloat *ptr = env->GetFloatArrayElements(meanValues, 0);
  vector<float> mean_values(ptr, ptr + num_channels);
  caffe_mobile->SetMean(mean_values);
}

JNIEXPORT void JNICALL
Java_strin_caffe_CaffeMobileOpenCL_setScale(JNIEnv *env,
                                                        jobject thiz,
                                                        jfloat scale) {
  CaffeMobile *caffe_mobile = CaffeMobile::Get();
  caffe_mobile->SetScale(scale);
}

JNIEXPORT jintArray JNICALL
Java_strin_caffe_CaffeMobileOpenCL_predictImage(JNIEnv *env,
                                                            jobject thiz,
                                                            jstring imgPath,
                                                            jint k) {
  CaffeMobile *caffe_mobile = CaffeMobile::Get();
  vector<int> top_k =
      caffe_mobile->PredictTopK(jstring2string(env, imgPath), k);

  jintArray result;
  result = env->NewIntArray(k);
  if (result == NULL) {
    return NULL; /* out of memory error thrown */
  }
  // move from the temp structure to the java structure
  env->SetIntArrayRegion(result, 0, k, &top_k[0]);
  return result;
}

JNIEXPORT jfloat JNICALL
Java_strin_caffe_CaffeMobileOpenCL_timePrediction(JNIEnv *env,
                                                            jobject thiz,
                                                            jobjectArray imgPaths) {
  CaffeMobile *caffe_mobile = CaffeMobile::Get();

  vector<string> newImgPaths;
  int stringCount = env->GetArrayLength(imgPaths);
  
  for(int i = 0; i < stringCount; i++) {
    jstring str = (jstring) env->GetObjectArrayElement(imgPaths, i);
    const char *rawString = env->GetStringUTFChars(str, 0);
    newImgPaths.push_back(string(rawString));
  }

  float result = caffe_mobile->timePrediction(newImgPaths);
  return result;
}

JNIEXPORT jobjectArray JNICALL
Java_strin_caffe_CaffeMobileOpenCL_extractFeatures(
    JNIEnv *env, jobject thiz, jstring imgPath, jstring blobNames) {
  CaffeMobile *caffe_mobile = CaffeMobile::Get();
  vector<vector<float>> features = caffe_mobile->ExtractFeatures(
      jstring2string(env, imgPath), jstring2string(env, blobNames));

  jobjectArray array2D =
      env->NewObjectArray(features.size(), env->FindClass("[F"), NULL);
  for (size_t i = 0; i < features.size(); ++i) {
    jfloatArray array1D = env->NewFloatArray(features[i].size());
    if (array1D == NULL) {
      return NULL; /* out of memory error thrown */
    }
    // move from the temp structure to the java structure
    env->SetFloatArrayRegion(array1D, 0, features[i].size(), &features[i][0]);
    env->SetObjectArrayElement(array2D, i, array1D);
  }
  return array2D;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
  JNIEnv *env = NULL;
  jint result = -1;

  if (vm->GetEnv((void **)&env, JNI_VERSION_1_6) != JNI_OK) {
    LOG(FATAL) << "GetEnv failed!";
    return result;
  }

  return JNI_VERSION_1_6;
}

#ifdef __cplusplus
}
#endif
