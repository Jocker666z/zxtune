/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class app_zxtune_core_jni_JniPlayer */

#ifndef _Included_app_zxtune_core_jni_JniPlayer
#define _Included_app_zxtune_core_jni_JniPlayer
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     app_zxtune_core_jni_JniPlayer
 * Method:    close
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_app_zxtune_core_jni_JniPlayer_close
  (JNIEnv *, jclass, jint);

/*
 * Class:     app_zxtune_core_jni_JniPlayer
 * Method:    getPlaybackPerformance
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL Java_app_zxtune_core_jni_JniPlayer_getPlaybackPerformance
  (JNIEnv *, jclass, jint);

/*
 * Class:     app_zxtune_core_jni_JniPlayer
 * Method:    render
 * Signature: ([S)Z
 */
JNIEXPORT jboolean JNICALL Java_app_zxtune_core_jni_JniPlayer_render
  (JNIEnv *, jobject, jshortArray);

/*
 * Class:     app_zxtune_core_jni_JniPlayer
 * Method:    analyze
 * Signature: ([B)I
 */
JNIEXPORT jint JNICALL Java_app_zxtune_core_jni_JniPlayer_analyze
  (JNIEnv *, jobject, jbyteArray);

/*
 * Class:     app_zxtune_core_jni_JniPlayer
 * Method:    getPosition
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_app_zxtune_core_jni_JniPlayer_getPosition
  (JNIEnv *, jobject);

/*
 * Class:     app_zxtune_core_jni_JniPlayer
 * Method:    setPosition
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_app_zxtune_core_jni_JniPlayer_setPosition
  (JNIEnv *, jobject, jint);

/*
 * Class:     app_zxtune_core_jni_JniPlayer
 * Method:    getProperty
 * Signature: (Ljava/lang/String;J)J
 */
JNIEXPORT jlong JNICALL Java_app_zxtune_core_jni_JniPlayer_getProperty__Ljava_lang_String_2J
  (JNIEnv *, jobject, jstring, jlong);

/*
 * Class:     app_zxtune_core_jni_JniPlayer
 * Method:    getProperty
 * Signature: (Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_app_zxtune_core_jni_JniPlayer_getProperty__Ljava_lang_String_2Ljava_lang_String_2
  (JNIEnv *, jobject, jstring, jstring);

/*
 * Class:     app_zxtune_core_jni_JniPlayer
 * Method:    setProperty
 * Signature: (Ljava/lang/String;J)V
 */
JNIEXPORT void JNICALL Java_app_zxtune_core_jni_JniPlayer_setProperty__Ljava_lang_String_2J
  (JNIEnv *, jobject, jstring, jlong);

/*
 * Class:     app_zxtune_core_jni_JniPlayer
 * Method:    setProperty
 * Signature: (Ljava/lang/String;Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_app_zxtune_core_jni_JniPlayer_setProperty__Ljava_lang_String_2Ljava_lang_String_2
  (JNIEnv *, jobject, jstring, jstring);

#ifdef __cplusplus
}
#endif
#endif