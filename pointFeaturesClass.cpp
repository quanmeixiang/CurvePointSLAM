#include "pointFeaturesClass.h"
#include "common.h"


//TODO: FIX UP CALIBRATION STUFF - the body2stereo and reverse don't reproduce the same thing
//TODO: FIX UP RANSAC COST STUFF - could only use 2 points, get a horribly wrong estimate but a good cost

using namespace std;

PointFeaturesClass::PointFeaturesClass()
{
        CvSize frame_size = cvSize(PIC_WIDTH,PIC_HEIGHT);

        eig_image = cvCreateImage( frame_size, IPL_DEPTH_32F, 1 );
        temp_image = cvCreateImage( frame_size, IPL_DEPTH_32F, 1 );
        pyramid1 = cvCreateImage(cvSize(PIC_WIDTH+8,PIC_HEIGHT/3), IPL_DEPTH_8U, 1);
        pyramid2 = cvCreateImage(cvSize(PIC_WIDTH+8,PIC_HEIGHT/3), IPL_DEPTH_8U, 1);
        //srand(0);
	//return;
        
        optical_flow_window = cvSize(25,25);
        optical_flow_termination_criteria = cvTermCriteria( CV_TERMCRIT_ITER | CV_TERMCRIT_EPS, 1000, .01 );
        
        num_pts_last = 0;
        num_pts_curr = 0;
}

PointFeaturesClass::~PointFeaturesClass()
{
        
        //optical_flow_window = cvSize(25,25);
        //optical_flow_termination_criteria = cvTermCriteria( CV_TERMCRIT_ITER | CV_TERMCRIT_EPS, 1000, .01 );
	//return;
}

void PointFeaturesClass::getPointMeasurements(IplImage ** last_image, IplImage ** image, double * meas_new, int * num_new_meas, double * meas_existing, int * num_existing_meas, int * correspondences)
{ 
    
timeval t_start, t_stop;
timeval start, stop;
float elapsedTime;
        for (int i = 0; i < existing_landmarks.size(); i++)
            existing_landmarks[i].frames_since_obs++;

        //First, copy 'current' to 'last' landmarks observed
        for (int i = 0; i < num_pts_curr; i++)
        {
            pts_last[LEFT][i] = pts_curr[LEFT][i];
            pts_last[RIGHT][i] = pts_curr[RIGHT][i];
            matches_last[i] = matches_curr[i];
        }
        num_pts_last = num_pts_curr;
        num_pts_curr = 0;
        gettimeofday(&start, NULL);
        trackExistingLandmarks(last_image, image, meas_existing, num_existing_meas);
        findNewLandmarks(image, meas_new, num_new_meas);
        //getStereoPts(image, stereo_pts, &num_total_meas);
        //determineDataAssoc(image, stereo_pts, num_total_meas, meas_new, num_new_meas, meas_existing, num_existing_meas, correspondences);
        
        
        
        
        
        gettimeofday(&stop, NULL);
        
	elapsedTime = (stop.tv_sec*1000.0 + stop.tv_usec/1000.0) -
		(start.tv_sec*1000.0 + start.tv_usec/1000.0);
        
        //cout << "Params: ";
        //for (int i = 0; i < 6; i++)
        //    cout << params[i] << " ";
        //cout << endl;
        //cout << "Elapsed Time: " << elapsedTime << endl;
        
                //Plots to debug
    CvFont font;
    cvInitFont(&font, CV_FONT_HERSHEY_SIMPLEX , 0.5f, 0.5f, 0, 1, 8);
    char tstring[5];
    for (int i = 0; i < *num_existing_meas; i++)
    {
        cvCircle(image[LEFT], cvPoint(meas_existing[4*i],meas_existing[4*i+1]), 5, CV_RGB(255,255,255), 1, CV_AA, 0 );
        sprintf(tstring,"%d",correspondences[i]);
        cvPutText(image[LEFT],tstring,cvPoint(meas_existing[4*i],meas_existing[4*i+1]-5),&font, CV_RGB(255,255,255));
        
        cvCircle(image[RIGHT], cvPoint(meas_existing[4*i+2],meas_existing[4*i+3]), 5, CV_RGB(255,255,255), 1, CV_AA, 0 );
        sprintf(tstring,"%d",correspondences[i]);
        cvPutText(image[RIGHT],tstring,cvPoint(meas_existing[4*i+2],meas_existing[4*i+3]-5),&font, CV_RGB(255,255,255));
    }
    for (int i = 0; i < *num_new_meas; i++)
    {
        cvCircle(image[LEFT], cvPoint(meas_new[4*i],meas_new[4*i+1]), 5, CV_RGB(0,0,0), 1, CV_AA, 0 );
        sprintf(tstring,"%d",-1);
        cvPutText(image[LEFT],tstring,cvPoint(meas_new[4*i],meas_new[4*i+1]-5),&font, CV_RGB(0,0,0));
        
        cvCircle(image[RIGHT], cvPoint(meas_new[4*i+2],meas_new[4*i+3]), 5, CV_RGB(0,0,0), 1, CV_AA, 0 );
        sprintf(tstring,"%d",-1);
        cvPutText(image[RIGHT],tstring,cvPoint(meas_new[4*i+2],meas_new[4*i+3]-5),&font, CV_RGB(0,0,0));
    }
    cvShowImage("Left",image[LEFT]);
    cvShowImage("Right",image[RIGHT]);
    cvWaitKey(0);
}

void PointFeaturesClass::getStereoPts(IplImage ** image, double * stereo_pts, int * num_features)
{
    //Find left image features
    CvPoint2D32f image_features_left[MAX_VODOM_FEATURES];
    int num_features_left = MAX_VODOM_FEATURES;
    cvGoodFeaturesToTrack(image[LEFT], eig_image, temp_image, image_features_left, &num_features_left, 0.01, 20, NULL, 13, 0, 0.04);
    
    
    //Find right image features
    CvPoint2D32f image_features_right[MAX_VODOM_FEATURES];
    int num_features_right = MAX_VODOM_FEATURES;
    cvGoodFeaturesToTrack(image[RIGHT], eig_image, temp_image, image_features_right, &num_features_right, 0.01, 20, NULL, 13, 0, 0.04);
    
    //cout << "Original features: " << num_last_features_left << " " << num_last_features_right << endl;
    
    //Remove ones that are too close to the edges
    for (int i = 0; i < num_features_left; i++)
    {
        while (image_features_left[i].x <= WINDOW_HALF || image_features_left[i].x >= PIC_WIDTH-WINDOW_HALF || image_features_left[i].y <= WINDOW_HALF || image_features_left[i].y >= PIC_HEIGHT-WINDOW_HALF )
        {
            image_features_left[i] =  image_features_left[num_features_left-1];
            num_features_left--;
            if (num_features_left <= 0)
                break;
        }
    }
    for (int i = 0; i < num_features_right; i++)
    {
        while (image_features_right[i].x <= WINDOW_HALF || image_features_right[i].x >= PIC_WIDTH-WINDOW_HALF || image_features_right[i].y <= WINDOW_HALF || image_features_right[i].y >= PIC_HEIGHT-WINDOW_HALF )
        {
            image_features_right[i] =  image_features_right[num_features_right-1];
            num_features_right--;
            if (num_features_right <= 0)
                break;
        }
    }
    /*  cout << "Edge removed Pts: " << endl;
    for (int i = 0; i < num_last_features_left; i++)
    {
        cout << i << ": " << last_image_features_left[i].x << " " << last_image_features_left[i].y << endl;
    }
    cout << endl;
    for (int i = 0; i < num_last_features_right; i++)
    {
        cout << i << ": " << last_image_features_right[i].x << " " << last_image_features_right[i].y << endl;
    }
    cout << endl;*/
    //cout << "Edge features removed: " << num_last_features_left << " " << num_last_features_right << endl;
    
    if (num_features_left == 0 || num_features_right == 0)
        num_features = 0;
    else
    {
        //Find stereo matches (compute SSDs and construct score matrix)
        //cout << num_last_features_left << " " << num_last_features_right << endl;
        CvMat * score = cvCreateMat(num_features_left,num_features_right,CV_32F);
        CvMat * bestmatch = cvCreateMat(num_features_left,num_features_right,CV_8U);
        int left_bestmatch[num_features_left];
        int right_bestmatch[num_features_right];
        for (int i = 0; i < num_features_left; i++)
        {
            for (int j = 0; j < num_features_right; j++)
            {
                CvPoint left_feat = cvPoint(image_features_left[i].x,image_features_left[i].y);
                CvPoint right_feat = cvPoint(image_features_right[j].x,image_features_right[j].y);

                if (abs(right_feat.y-left_feat.y) > EPI_THRESH || left_feat.x-right_feat.x >= DISP_THRESH_HI || left_feat.x-right_feat.x <= DISP_THRESH_LO)
                    cvmSet(score,i,j,1000000.0);
                else
                {
                    cvSetImageROI(image[LEFT],cvRect(left_feat.x-WINDOW_HALF,left_feat.y-WINDOW_HALF,WINDOW_SIZE,WINDOW_SIZE));
                    cvSetImageROI(image[RIGHT],cvRect(right_feat.x-WINDOW_HALF,right_feat.y-WINDOW_HALF,WINDOW_SIZE,WINDOW_SIZE));
                    //cout << left_feat.x << " " << left_feat.y << endl;
                    //cout << right_feat.x << " " << right_feat.y << endl;

                    cvmSet(score,i,j,cvNorm(image[LEFT],image[RIGHT]));

                    cvResetImageROI(image[LEFT]);
                    cvResetImageROI(image[RIGHT]);
                }

            }
        }

        //Find left_bestmatches
        for (int i = 0; i < num_features_left; i++)
        {
            left_bestmatch[i] = -1;
            double current_best_cost = SSD_THRESH;
            for (int j = 0; j < num_features_right; j++)
            {
                CvPoint left_feat = cvPoint(image_features_left[i].x,image_features_left[i].y);
                CvPoint right_feat = cvPoint(image_features_right[j].x,image_features_right[j].y);

                if (abs(right_feat.y-left_feat.y) <= EPI_THRESH)
                {
                    double ssd = cvmGet(score,i,j);
                    if(ssd < current_best_cost)
                    {
                        current_best_cost = ssd;
                        left_bestmatch[i] = j;
                    }
                }
            }
        }
        //Find right_bestmatches
        for (int j = 0; j < num_features_right; j++)
        {
            right_bestmatch[j] = -1;
            double current_best_cost = SSD_THRESH;
            for (int i = 0; i < num_features_left; i++)
            {
                CvPoint left_feat = cvPoint(image_features_left[i].x,image_features_left[i].y);
                CvPoint right_feat = cvPoint(image_features_right[j].x,image_features_right[j].y);

                if (abs(right_feat.y-left_feat.y) <= EPI_THRESH)
                {
                    double ssd = cvmGet(score,i,j);
                    if(ssd < current_best_cost)
                    {
                        current_best_cost = ssd;
                        right_bestmatch[j] = i;
                    }
                }
            }
        }

        //Only keep ones that share mutual matches and whose disparity is >0

        CvPoint2D32f matched_features_left[MAX_VODOM_FEATURES];
        CvPoint2D32f matched_features_right[MAX_VODOM_FEATURES];
        int k = 0;

        for (int i = 0; i < num_features_left; i++)
        {
            if (left_bestmatch[i] != -1 && right_bestmatch[left_bestmatch[i]] == i && (image_features_left[i].x - image_features_right[left_bestmatch[i]].x) > -2*POINT_OFFSET)
            {
                matched_features_left[k] =  image_features_left[i];
                matched_features_right[k] =  image_features_right[left_bestmatch[i]];
                k++;
            }
        }

        int num_matched_features = k;

        //Copy to output vectors
        for (int i = 0; i < num_matched_features; i++)
        {
            stereo_pts[4*i] = matched_features_left[i].x;//+POINT_OFFSET;
            stereo_pts[4*i+1] = matched_features_left[i].y;
            stereo_pts[4*i+2] = matched_features_right[i].x;//-POINT_OFFSET;
            stereo_pts[4*i+3] = matched_features_right[i].y;
        }
        *num_features = num_matched_features;    

        //cout << "Num stereo matched features: " << num_matched_features << endl;
    }
}

void PointFeaturesClass::determineDataAssoc(IplImage ** image, double * measurements, int num_features, double * meas_new, int * num_new_meas, double * meas_existing, int * num_existing_meas, int * correspondences)
{
    std::vector<IplImage *> meas_patches;
    const int num_landmarks = existing_landmarks.size();
    bool landmarks_allocated[num_landmarks];
    memset( landmarks_allocated, false, num_landmarks*sizeof(bool) );
    for (int i = 0; i < num_features; i++)
        cout << measurements[4*i] << " " << measurements[4*i+1] << " " << measurements[4*i+2] << " " << measurements[4*i+3] << endl;
    
    int num_ex = 0;
    int num_new = 0;
    //For each measurement:
    for (int i = 0; i < num_features; i++)
    {
        //Assume it's a new landmark to begin with
        int current_corr = -1;
        
        //Extract the patch using L and R image coords
        IplImage * current_patch = cvCreateImage(cvSize(PATCH_SIZE,PATCH_SIZE),IPL_DEPTH_8U,1);
        cvSetImageROI(image[LEFT],cvRect(measurements[4*i]-PATCH_HALF,measurements[4*i+1]-PATCH_HALF,PATCH_SIZE,PATCH_SIZE));
        cvSetImageROI(image[RIGHT],cvRect(measurements[4*i+2]-PATCH_HALF,measurements[4*i+3]-PATCH_HALF,PATCH_SIZE,PATCH_SIZE));
        cvAddWeighted(image[LEFT], 0.5, image[RIGHT], 0.5, 0.0, current_patch);
        meas_patches.push_back(current_patch);
        cvResetImageROI(image[LEFT]);
        cvResetImageROI(image[RIGHT]);
        
        //Compare to existing landmarks (patches='codebook'?)
        //Preference is given to more recent landmarks
        double mindiff = DA_SSD_THRESH;
        int num_landmarks = existing_landmarks.size();
        cout << existing_landmarks.size() << endl;
        for (int j = num_landmarks-1; j >= 0; j--)
        {
            //Reject those that have already been allocated
            if (!landmarks_allocated[j])
            {
                //Reject any if too far away from last sighting (taking into account frames_since_obs)
                double dist_to_lm = 0.5*pow( pow( existing_landmarks[j].last_meas[0]-measurements[4*i] , 2.0) + pow( existing_landmarks[j].last_meas[1]-measurements[4*i+1] , 2.0), 0.5) + 0.5*pow( pow( existing_landmarks[j].last_meas[2]-measurements[4*i+2] , 2.0) + pow( existing_landmarks[j].last_meas[3]-measurements[4*i+3] , 2.0), 0.5);
                if(dist_to_lm < existing_landmarks[j].frames_since_obs*LM_MOTION_THRESH)
                {
                    //Pick best one, if below threshold
                    double ssd = cvNorm(current_patch,existing_landmarks[j].patches);
                    if (ssd < mindiff)
                    {
                        mindiff = ssd;
                        current_corr = j;
                    }
                }
            }
        }
        
        //If not matched, add to 'new' list, then add the patch and meas to existing_landmarks
        if (current_corr == -1)
        {
            for (int j = 0; j < 4; j++)
                meas_new[4*num_new+j] = measurements[4*i+j];
            num_new++;
            
            std::vector<double> meas;
            for (int j = 0; j < 4; j++)
                meas.push_back(measurements[4*i+j]);
            landmarkStruct * newlm = new(landmarkStruct);
            newlm->patches = meas_patches[i];
            newlm->last_meas = meas;
            newlm->times_obs = 1;
            newlm->frames_since_obs = 0;
            existing_landmarks.push_back(*newlm);
        }
        //Otherwise, add to 'existing' list and specify landmark number too, 
        //copy over the last measurement and update the stored patch
        else
        {
            correspondences[num_ex] = current_corr;
            landmarks_allocated[current_corr] = true;
            for (int j = 0; j < 4; j++)
                meas_existing[4*num_ex+j] = measurements[4*i+j];
            num_ex++;
            
            for (int j = 0; j < 4; j++)
                existing_landmarks[current_corr].last_meas[j] = measurements[4*i+j];
            existing_landmarks[current_corr].times_obs++;
            existing_landmarks[current_corr].frames_since_obs = 0;
            double ratio1 = (double)(existing_landmarks[current_corr].times_obs-1)/existing_landmarks[current_corr].times_obs;
            double ratio2 = 1.0/existing_landmarks[current_corr].times_obs;
            cvAddWeighted(existing_landmarks[current_corr].patches, ratio1, meas_patches[i], ratio2, 0.0, existing_landmarks[current_corr].patches);
        
        }
    }

    //For all existing landmarks, copy over the last measurement and update the stored patch
    for (int i = 0; i < num_ex; i++)
    {
        for (int j = 0; j < 4; j++)
            existing_landmarks[correspondences[i]].last_meas[j] = measurements[4*i+j];
        existing_landmarks[correspondences[i]].times_obs++;
        existing_landmarks[correspondences[i]].frames_since_obs = 0;
        double ratio1 = (double)(existing_landmarks[correspondences[i]].times_obs-1)/existing_landmarks[correspondences[i]].times_obs;
        double ratio2 = 1.0/existing_landmarks[correspondences[i]].times_obs;
        cvAddWeighted(existing_landmarks[correspondences[i]].patches, ratio1, meas_patches[i], ratio2, 0.0, existing_landmarks[correspondences[i]].patches);
        
    }
    
    *num_existing_meas = num_ex;
    *num_new_meas = num_new;
}


void PointFeaturesClass::trackExistingLandmarks(IplImage ** last_image, IplImage ** image, double * meas_existing, int *num_existing_meas)
{
    //Only bother tracking if we have any previous viewed landmarks
    if(num_pts_last)
    {
        //Track left and right points to current image
        cvCalcOpticalFlowPyrLK(last_image[LEFT], image[LEFT], pyramid1, pyramid2, &(pts_last[LEFT][0]),&(temp_pts_curr[LEFT][0]), num_pts_last, optical_flow_window, 3,optical_flow_found_feature_left, optical_flow_feature_error_left,optical_flow_termination_criteria, 0 );
        cvCalcOpticalFlowPyrLK(last_image[RIGHT], image[RIGHT], pyramid1, pyramid2, &(pts_last[RIGHT][0]),&(temp_pts_curr[RIGHT][0]), num_pts_last, optical_flow_window, 3,optical_flow_found_feature_right, optical_flow_feature_error_right,optical_flow_termination_criteria, 0 );
        
        num_pts_curr = 0;
        num_existing_meas = 0;
        //Check tracked points
        //Remove any that don't seem right (disparity), or are bad tracks (!feature_found), or have SSD above a thresh
        //Copy the rest over to the pts_curr vector and also to meas_existing vector
        for (int i = 0; i < num_pts_last; i++)
        {
            //If tracked OK
            if(optical_flow_found_feature_left[i] && optical_flow_found_feature_right[i])
            {
                //If disparity OK
                if((temp_pts_curr[LEFT][i].x-temp_pts_curr[RIGHT][i].x) > 0.0 && (temp_pts_curr[LEFT][i].x-temp_pts_curr[RIGHT][i].x) < 50.0)
                {
                    //If not too close to the edge
                    if(temp_pts_curr[LEFT][i].x > PATCH_HALF && temp_pts_curr[LEFT][i].x < PIC_WIDTH-PATCH_HALF && temp_pts_curr[LEFT][i].y > PATCH_HALF && temp_pts_curr[LEFT][i].y < PIC_HEIGHT-PATCH_HALF)
                    {
                        //If SSD matches reasonably between tracked features for both images
                        cvSetImageROI(image[LEFT],cvRect(temp_pts_curr[LEFT][i].x-PATCH_HALF,temp_pts_curr[LEFT][i].y-PATCH_HALF,PATCH_SIZE,PATCH_SIZE));
                        cvSetImageROI(image[RIGHT],cvRect(temp_pts_curr[RIGHT][i].x-PATCH_HALF,temp_pts_curr[RIGHT][i].x-PATCH_HALF,PATCH_SIZE,PATCH_SIZE));

                        if(cvNorm(image[LEFT],image[RIGHT]) < SSD_THRESH)
                        {
                            pts_curr[LEFT][num_pts_curr] = temp_pts_curr[LEFT][i];
                            pts_curr[RIGHT][num_pts_curr] = temp_pts_curr[RIGHT][i];
                            
                            meas_existing[4*num_pts_curr] = temp_pts_curr[LEFT][i].x;
                            meas_existing[4*num_pts_curr+1] = temp_pts_curr[LEFT][i].y;
                            meas_existing[4*num_pts_curr+2] = temp_pts_curr[RIGHT][i].x;
                            meas_existing[4*num_pts_curr+3] = temp_pts_curr[RIGHT][i].y;
                            num_pts_curr++;
                        }
                        cvResetImageROI(image[LEFT]);
                        cvResetImageROI(image[RIGHT]);
                    }
                }
            }
        }
        *num_existing_meas = num_pts_curr;
    }
    
}

void PointFeaturesClass::findNewLandmarks(IplImage ** image, double * meas_new, int *num_new_meas)
{
    //Only bother if we don't have the maximum allowed # landmarks already
    if (num_pts_curr < NUM_CURRENT_LANDMARKS)
    {
        //Find left image features
        CvPoint2D32f image_features_left[MAX_VODOM_FEATURES];
        int num_features_left = MAX_VODOM_FEATURES;
        cvGoodFeaturesToTrack(image[LEFT], eig_image, temp_image, image_features_left, &num_features_left, 0.01, 20, NULL, 13, 0, 0.04);
        //Find right image features
        CvPoint2D32f image_features_right[MAX_VODOM_FEATURES];
        int num_features_right = MAX_VODOM_FEATURES;
        cvGoodFeaturesToTrack(image[RIGHT], eig_image, temp_image, image_features_right, &num_features_right, 0.01, 20, NULL, 13, 0, 0.04);

        //Remove ones that are too close to the edges
        for (int i = 0; i < num_features_left; i++)
        {
            while (image_features_left[i].x <= WINDOW_HALF || image_features_left[i].x >= PIC_WIDTH-WINDOW_HALF || image_features_left[i].y <= WINDOW_HALF || image_features_left[i].y >= PIC_HEIGHT-WINDOW_HALF )
            {
                image_features_left[i] =  image_features_left[num_features_left-1];
                num_features_left--;
                if (num_features_left <= 0)
                    break;
            }
        }
        for (int i = 0; i < num_features_right; i++)
        {
            while (image_features_right[i].x <= WINDOW_HALF || image_features_right[i].x >= PIC_WIDTH-WINDOW_HALF || image_features_right[i].y <= WINDOW_HALF || image_features_right[i].y >= PIC_HEIGHT-WINDOW_HALF )
            {
                image_features_right[i] =  image_features_right[num_features_right-1];
                num_features_right--;
                if (num_features_right <= 0)
                    break;
            }
        }
        
        //Remove ones that are sufficiently close to the ones we have already
        for (int j = 0; j < num_pts_curr; j++)
        {
            for (int i = 0; i < num_features_left; i++)
            {
                while (DIST(image_features_left[i],pts_curr[LEFT][j]) < LM_PROXIMITY_THRESH )
                {
                    image_features_left[i] =  image_features_left[num_features_left-1];
                    num_features_left--;
                    if (num_features_left <= 0)
                        break;
                }
            }
            for (int i = 0; i < num_features_right; i++)
            {
                while (DIST(image_features_right[i],pts_curr[RIGHT][j]) < LM_PROXIMITY_THRESH )
                {
                    image_features_right[i] =  image_features_right[num_features_right-1];
                    num_features_right--;
                    if (num_features_right <= 0)
                        break;
                }
            }
        }


        if (num_features_left != 0 && num_features_right != 0)
        {
            //Find stereo matches (compute SSDs and construct score matrix)
            //cout << num_last_features_left << " " << num_last_features_right << endl;
            CvMat * score = cvCreateMat(num_features_left,num_features_right,CV_32F);
            int left_bestmatch[num_features_left];
            int right_bestmatch[num_features_right];
            for (int i = 0; i < num_features_left; i++)
            {
                for (int j = 0; j < num_features_right; j++)
                {
                    CvPoint left_feat = cvPoint(image_features_left[i].x,image_features_left[i].y);
                    CvPoint right_feat = cvPoint(image_features_right[j].x,image_features_right[j].y);

                    if (abs(right_feat.y-left_feat.y) > EPI_THRESH || left_feat.x-right_feat.x >= DISP_THRESH_HI || left_feat.x-right_feat.x <= DISP_THRESH_LO)
                        cvmSet(score,i,j,1000000.0);
                    else
                    {
                        cvSetImageROI(image[LEFT],cvRect(left_feat.x-WINDOW_HALF,left_feat.y-WINDOW_HALF,WINDOW_SIZE,WINDOW_SIZE));
                        cvSetImageROI(image[RIGHT],cvRect(right_feat.x-WINDOW_HALF,right_feat.y-WINDOW_HALF,WINDOW_SIZE,WINDOW_SIZE));
                        //cout << left_feat.x << " " << left_feat.y << endl;
                        //cout << right_feat.x << " " << right_feat.y << endl;
                        cvmSet(score,i,j,cvNorm(image[LEFT],image[RIGHT]));

                        cvResetImageROI(image[LEFT]);
                        cvResetImageROI(image[RIGHT]);
                    }
                }
            }

            //Find left_bestmatches
            for (int i = 0; i < num_features_left; i++)
            {
                left_bestmatch[i] = -1;
                double current_best_cost = SSD_THRESH;
                for (int j = 0; j < num_features_right; j++)
                {
                    CvPoint left_feat = cvPoint(image_features_left[i].x,image_features_left[i].y);
                    CvPoint right_feat = cvPoint(image_features_right[j].x,image_features_right[j].y);

                    if (abs(right_feat.y-left_feat.y) <= EPI_THRESH)
                    {
                        double ssd = cvmGet(score,i,j);
                        if(ssd < current_best_cost)
                        {
                            current_best_cost = ssd;
                            left_bestmatch[i] = j;
                        }
                    }
                }
            }
            //Find right_bestmatches
            for (int j = 0; j < num_features_right; j++)
            {
                right_bestmatch[j] = -1;
                double current_best_cost = SSD_THRESH;
                for (int i = 0; i < num_features_left; i++)
                {
                    CvPoint left_feat = cvPoint(image_features_left[i].x,image_features_left[i].y);
                    CvPoint right_feat = cvPoint(image_features_right[j].x,image_features_right[j].y);

                    if (abs(right_feat.y-left_feat.y) <= EPI_THRESH)
                    {
                        double ssd = cvmGet(score,i,j);
                        if(ssd < current_best_cost)
                        {
                            current_best_cost = ssd;
                            right_bestmatch[j] = i;
                        }
                    }
                }
            }

            //Only keep ones that share mutual matches and whose disparity is >0

            CvPoint2D32f matched_features_left[MAX_VODOM_FEATURES];
            CvPoint2D32f matched_features_right[MAX_VODOM_FEATURES];
            int k = 0;

            for (int i = 0; i < num_features_left; i++)
            {
                if (left_bestmatch[i] != -1 && right_bestmatch[left_bestmatch[i]] == i && (image_features_left[i].x - image_features_right[left_bestmatch[i]].x) > -2*POINT_OFFSET)
                {
                    matched_features_left[k] =  image_features_left[i];
                    matched_features_right[k] =  image_features_right[left_bestmatch[i]];
                    k++;
                }
            }

            int num_features_to_add = MIN(k,NUM_CURRENT_LANDMARKS - num_pts_curr);
            int n_new_pts = 0;
            
            //Copy whatever we need (only to make up the rest) to pts_curr and meas_new
            for (int i = 0; i < num_features_to_add; i++)
            {
                pts_curr[LEFT][num_pts_curr] = matched_features_left[i];
                pts_curr[RIGHT][num_pts_curr] = matched_features_right[i];

                meas_new[4*n_new_pts] = matched_features_left[i].x;
                meas_new[4*n_new_pts+1] = matched_features_left[i].y;
                meas_new[4*n_new_pts+2] = matched_features_right[i].x;
                meas_new[4*n_new_pts+3] = matched_features_right[i].y;
                num_pts_curr++;
                n_new_pts++;
            } 
            *num_new_meas = n_new_pts;
            //cout << "Num stereo matched features: " << num_matched_features << endl;
        }
    }
}