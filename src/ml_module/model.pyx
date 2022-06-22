# cython: language_level = 3
# distutils: language = c++

import numpy as np
from sklearn.ensemble import RandomForestClassifier
from sklearn.neighbors import KNeighborsClassifier
from sklearn.linear_model import SGDClassifier
import os
import joblib
import math
import collections
import time
import prettytable as pt

class KNC(object):
    def __init__(self, n_neighbors=21, n_records=130, min_records_per_label=46, p=1):
        self.knc = KNeighborsClassifier(n_neighbors=n_neighbors, p=p)
        self.x = [collections.deque(maxlen=n_records), collections.deque(maxlen=n_records)]
        self.time = [collections.deque(), collections.deque()]
        self.k = n_neighbors
        self.n_records = max(n_records, self.k)
        self.clock = self.n_records
        self.name_ = "knc"
        self.il_ = True
        self.training_time = 0
        self.training_num = 0
        self.testing_time = 0
        self.testing_num = 0
        self.min_records_per_label = min(max(math.ceil(self.k / 2), min_records_per_label), math.ceil(self.n_records / 2))
        print("k=%d, n_records=%d, min_records_per_label=%d" %(self.k, self.n_records, self.min_records_per_label))
    def train(self, record, label):
        self.x[label].append(record.copy())
        self.time[label].append(self.next_clock())
        if len(self.x[label]) + len(self.x[1 - label]) < self.n_records:
            self.x[1 - label].append(record.copy())
            self.time[1 - label].append(self.next_clock())
        elif len(self.x[label]) + len(self.x[1 - label]) > self.n_records:
            del_label = label
            if len(self.x[0]) > self.min_records_per_label and len(self.x[1]) > self.min_records_per_label:
                del_label = 0 if self.time[0][0] < self.time[1][0] else 1
            self.x[del_label].popleft()
            self.time[del_label].popleft()
    def predict(self, x_test):
        if len(self.x[0]) + len(self.x[1]) < self.n_records:
            return int(1)
        y = [0] * len(self.x[0]) + [1] * len(self.x[1])
        x = self.x[0] + self.x[1]
        return int(self.knc.fit(x, y).predict([x_test])[0])
    def next_clock(self):
        self.clock = (self.clock + 1) % (self.n_records + 1)
        return self.clock

class RFC(object):
    def __init__(self, pre_trained_model='./src/ml_module/rfc.joblib'):
        self.rfc = joblib.load(pre_trained_model)
        self.il_ = False
        self.name_ = "rfc"
        self.training_time = 0
        self.training_num = 0
        self.testing_time = 0
        self.testing_num = 0
        print("Random Forest Classifier initialized with pre trained model: " + pre_trained_model)
    def predict(self, x_test):
        return int(self.rfc.predict([x_test])[0])

class LinearSVC(object):
    def __init__(self, pre_trained_model='./src/ml_module/lsvc.joblib'):
        self.lsvc = joblib.load(pre_trained_model)
        self.il_ = False
        self.name_ = "lsvc"
        self.training_time = 0
        self.training_num = 0
        self.testing_time = 0
        self.testing_num = 0
        print("Linear SVC initialized with pre trained model: " + pre_trained_model)
    def predict(self, x_test):
        return int(self.lsvc.predict([x_test])[0])

class LogisticRegression(object):
    def __init__(self, pre_trained_model='./src/ml_module/lr.joblib'):
        self.lr = joblib.load(pre_trained_model)
        self.il_ = False
        self.name_ = "lr"
        self.training_time = 0
        self.training_num = 0
        self.testing_time = 0
        self.testing_num = 0
        print("Logistic Regression initialized with pre trained model: " + pre_trained_model)
    def predict(self, x_test):
        return int(self.lr.predict([x_test])[0])

class IncrementalLinearSVC(object):
    def __init__(self, pre_trained_model='./src/ml_module/ilsvc.joblib'):
        self.ilsvc = joblib.load(pre_trained_model)
        params = {"warm_start": True}
        self.ilsvc.set_params(**params)
        self.il_ = True
        self.name_ = "ilsvc"
        self.training_time = 0
        self.training_num = 0
        self.testing_time = 0
        self.testing_num = 0
        print("Incremental Linear SVC initialized with pre trained model: " + pre_trained_model)
    def train(self, x, y):
        self.ilsvc.partial_fit([x], [y])
    def predict(self, x_test):
        return int(self.ilsvc.predict([x_test])[0])

class IncrementalLogisticRegression(object):
    def __init__(self, pre_trained_model='./src/ml_module/ilr.joblib'):
        self.ilr = joblib.load(pre_trained_model)
        params = {"warm_start": True}
        self.ilr.set_params(**params)
        self.il_ = True
        self.name_ = "ilr"
        self.training_time = 0
        self.training_num = 0
        self.testing_time = 0
        self.testing_num = 0
        print("Incremental Logistic Regression initialized with pre trained model: " + pre_trained_model)
    def train(self, x, y):
        self.ilr.partial_fit([x], [y])
    def predict(self, x_test):
        return int(self.ilr.predict([x_test])[0])

def load_clf(model_name, pre_trained_model):
    model_name = str(model_name, encoding = "utf8")
    pre_trained_model = str(pre_trained_model, encoding = "utf8")
    if os.path.exists(pre_trained_model):
        if model_name == "rfc": return RFC(pre_trained_model=pre_trained_model)
        elif model_name == "lsvc": return LinearSVC(pre_trained_model=pre_trained_model)
        elif model_name == "lr": return LogisticRegression(pre_trained_model=pre_trained_model)
        elif model_name == "ilsvc": return IncrementalLinearSVC(pre_trained_model=pre_trained_model)
        elif model_name == "ilr": return IncrementalLogisticRegression(pre_trained_model=pre_trained_model)
    print("%s initialized failed, pre trained model %s doesn't exists" %(model_name, pre_trained_model))
    return None
def incremental_learning(clf, x, y):
    if clf.il_:
        tik = time.time()
        clf.train(x, y)
        tok = time.time()
        clf.training_time += (tok - tik) * 1e6
        clf.training_num += 1
        return True
    else:
        #print("%s does not support incremental learning" %clf.name_)
        return False
def clf_predict(clf, x):
    tik = time.time()
    pred = clf.predict(x)
    tok = time.time()
    clf.testing_time += (tok - tik) * 1e6
    clf.testing_num += 1
    return pred

cdef public object load(const char* model_name, const char* pre_trained_model):
    return load_clf(model_name, pre_trained_model)
cdef public bint predict(object clf, float bip, int gt, int ut, float psd, float f):
    x = [bip, gt, ut, psd, f]
    return clf_predict(clf, x)
cdef public bint train(object clf, float bip, int gt, int ut, float psd, float f, int gc):
    x = [bip, gt, ut, psd, f]
    return incremental_learning(clf, x, gc)

cdef public object load_knc(int n_neighbors, int n_records, int min_records_per_label):
    return KNC(n_neighbors=n_neighbors, n_records=n_records, min_records_per_label=min_records_per_label)

cdef public void testing():
    print("testing")
cdef public void delete_classifier(object classifier):
    del classifier
    print('delete gc_classifier success')
cdef public void timing_information(object classifier):
    tb = pt.PrettyTable()
    tb.field_names = ['ml timing information', 'train', 'test']
    tb.add_row(['time (us)', int(classifier.training_time), int(classifier.testing_time)])
    tb.add_row(['num', classifier.training_num, classifier.testing_num])
    tb.add_row(['time per num (us)', int(classifier.training_time / (classifier.training_num + 1e-10)), 
                int(classifier.testing_time / (classifier.testing_num + 1e-10))])
    classifier.training_time = 0
    classifier.training_num = 0
    classifier.testing_time = 0
    classifier.testing_num = 0
    print(tb)
