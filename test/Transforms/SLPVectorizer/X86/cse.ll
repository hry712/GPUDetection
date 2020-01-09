; NOTE: Assertions have been autogenerated by utils/update_test_checks.py
; RUN: opt < %s -basicaa -slp-vectorizer -S -mtriple=i386-apple-macosx10.8.0 -mcpu=corei7-avx | FileCheck %s

target datalayout = "e-p:32:32:32-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:32:64-f32:32:32-f64:32:64-v64:64:64-v128:128:128-a0:0:64-f80:128:128-n8:16:32-S128"
target triple = "i386-apple-macosx10.8.0"

;int test(double *G) {
;  G[0] = 1+G[5]*4;
;  G[1] = 6+G[6]*3;
;  G[2] = 7+G[5]*4;
;  G[3] = 8+G[6]*4;
;}

define i32 @test(double* nocapture %G) {
; CHECK-LABEL: @test(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[ARRAYIDX:%.*]] = getelementptr inbounds double, double* [[G:%.*]], i64 5
; CHECK-NEXT:    [[ARRAYIDX2:%.*]] = getelementptr inbounds double, double* [[G]], i64 6
; CHECK-NEXT:    [[TMP0:%.*]] = bitcast double* [[ARRAYIDX]] to <2 x double>*
; CHECK-NEXT:    [[TMP1:%.*]] = load <2 x double>, <2 x double>* [[TMP0]], align 8
; CHECK-NEXT:    [[TMP2:%.*]] = fmul <2 x double> [[TMP1]], <double 4.000000e+00, double 3.000000e+00>
; CHECK-NEXT:    [[TMP3:%.*]] = fadd <2 x double> [[TMP2]], <double 1.000000e+00, double 6.000000e+00>
; CHECK-NEXT:    [[ARRAYIDX5:%.*]] = getelementptr inbounds double, double* [[G]], i64 1
; CHECK-NEXT:    [[TMP4:%.*]] = bitcast double* [[G]] to <2 x double>*
; CHECK-NEXT:    store <2 x double> [[TMP3]], <2 x double>* [[TMP4]], align 8
; CHECK-NEXT:    [[TMP5:%.*]] = extractelement <2 x double> [[TMP2]], i32 0
; CHECK-NEXT:    [[ARRAYIDX9:%.*]] = getelementptr inbounds double, double* [[G]], i64 2
; CHECK-NEXT:    [[TMP6:%.*]] = extractelement <2 x double> [[TMP1]], i32 1
; CHECK-NEXT:    [[MUL11:%.*]] = fmul double [[TMP6]], 4.000000e+00
; CHECK-NEXT:    [[TMP7:%.*]] = insertelement <2 x double> undef, double [[TMP5]], i32 0
; CHECK-NEXT:    [[TMP8:%.*]] = insertelement <2 x double> [[TMP7]], double [[MUL11]], i32 1
; CHECK-NEXT:    [[TMP9:%.*]] = fadd <2 x double> [[TMP8]], <double 7.000000e+00, double 8.000000e+00>
; CHECK-NEXT:    [[ARRAYIDX13:%.*]] = getelementptr inbounds double, double* [[G]], i64 3
; CHECK-NEXT:    [[TMP10:%.*]] = bitcast double* [[ARRAYIDX9]] to <2 x double>*
; CHECK-NEXT:    store <2 x double> [[TMP9]], <2 x double>* [[TMP10]], align 8
; CHECK-NEXT:    ret i32 undef
;
entry:
  %arrayidx = getelementptr inbounds double, double* %G, i64 5
  %0 = load double, double* %arrayidx, align 8
  %mul = fmul double %0, 4.000000e+00
  %add = fadd double %mul, 1.000000e+00
  store double %add, double* %G, align 8
  %arrayidx2 = getelementptr inbounds double, double* %G, i64 6
  %1 = load double, double* %arrayidx2, align 8
  %mul3 = fmul double %1, 3.000000e+00
  %add4 = fadd double %mul3, 6.000000e+00
  %arrayidx5 = getelementptr inbounds double, double* %G, i64 1
  store double %add4, double* %arrayidx5, align 8
  %add8 = fadd double %mul, 7.000000e+00
  %arrayidx9 = getelementptr inbounds double, double* %G, i64 2
  store double %add8, double* %arrayidx9, align 8
  %mul11 = fmul double %1, 4.000000e+00
  %add12 = fadd double %mul11, 8.000000e+00
  %arrayidx13 = getelementptr inbounds double, double* %G, i64 3
  store double %add12, double* %arrayidx13, align 8
  ret i32 undef
}

;int foo(double *A, int n) {
;  A[0] = A[0] * 7.9 * n + 6.0;
;  A[1] = A[1] * 7.7 * n + 2.0;
;  A[2] = A[2] * 7.6 * n + 3.0;
;  A[3] = A[3] * 7.4 * n + 4.0;
;}

define i32 @foo(double* nocapture %A, i32 %n) {
; CHECK-LABEL: @foo(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[CONV:%.*]] = sitofp i32 [[N:%.*]] to double
; CHECK-NEXT:    [[ARRAYIDX3:%.*]] = getelementptr inbounds double, double* [[A:%.*]], i64 1
; CHECK-NEXT:    [[ARRAYIDX9:%.*]] = getelementptr inbounds double, double* [[A]], i64 2
; CHECK-NEXT:    [[ARRAYIDX15:%.*]] = getelementptr inbounds double, double* [[A]], i64 3
; CHECK-NEXT:    [[TMP0:%.*]] = bitcast double* [[A]] to <4 x double>*
; CHECK-NEXT:    [[TMP1:%.*]] = load <4 x double>, <4 x double>* [[TMP0]], align 8
; CHECK-NEXT:    [[TMP2:%.*]] = fmul <4 x double> [[TMP1]], <double 7.900000e+00, double 7.700000e+00, double 7.600000e+00, double 7.400000e+00>
; CHECK-NEXT:    [[TMP3:%.*]] = insertelement <4 x double> undef, double [[CONV]], i32 0
; CHECK-NEXT:    [[TMP4:%.*]] = insertelement <4 x double> [[TMP3]], double [[CONV]], i32 1
; CHECK-NEXT:    [[TMP5:%.*]] = insertelement <4 x double> [[TMP4]], double [[CONV]], i32 2
; CHECK-NEXT:    [[TMP6:%.*]] = insertelement <4 x double> [[TMP5]], double [[CONV]], i32 3
; CHECK-NEXT:    [[TMP7:%.*]] = fmul <4 x double> [[TMP6]], [[TMP2]]
; CHECK-NEXT:    [[TMP8:%.*]] = fadd <4 x double> [[TMP7]], <double 6.000000e+00, double 2.000000e+00, double 3.000000e+00, double 4.000000e+00>
; CHECK-NEXT:    [[TMP9:%.*]] = bitcast double* [[A]] to <4 x double>*
; CHECK-NEXT:    store <4 x double> [[TMP8]], <4 x double>* [[TMP9]], align 8
; CHECK-NEXT:    ret i32 undef
;
entry:
  %0 = load double, double* %A, align 8
  %mul = fmul double %0, 7.900000e+00
  %conv = sitofp i32 %n to double
  %mul1 = fmul double %conv, %mul
  %add = fadd double %mul1, 6.000000e+00
  store double %add, double* %A, align 8
  %arrayidx3 = getelementptr inbounds double, double* %A, i64 1
  %1 = load double, double* %arrayidx3, align 8
  %mul4 = fmul double %1, 7.700000e+00
  %mul6 = fmul double %conv, %mul4
  %add7 = fadd double %mul6, 2.000000e+00
  store double %add7, double* %arrayidx3, align 8
  %arrayidx9 = getelementptr inbounds double, double* %A, i64 2
  %2 = load double, double* %arrayidx9, align 8
  %mul10 = fmul double %2, 7.600000e+00
  %mul12 = fmul double %conv, %mul10
  %add13 = fadd double %mul12, 3.000000e+00
  store double %add13, double* %arrayidx9, align 8
  %arrayidx15 = getelementptr inbounds double, double* %A, i64 3
  %3 = load double, double* %arrayidx15, align 8
  %mul16 = fmul double %3, 7.400000e+00
  %mul18 = fmul double %conv, %mul16
  %add19 = fadd double %mul18, 4.000000e+00
  store double %add19, double* %arrayidx15, align 8
  ret i32 undef
}

; int test2(double *G, int k) {
;   if (k) {
;     G[0] = 1+G[5]*4;
;     G[1] = 6+G[6]*3;
;   } else {
;     G[2] = 7+G[5]*4;
;     G[3] = 8+G[6]*3;
;   }
; }

; We can't merge the gather sequences because one does not dominate the other.

define i32 @test2(double* nocapture %G, i32 %k) {
; CHECK-LABEL: @test2(
; CHECK-NEXT:    [[TMP1:%.*]] = icmp eq i32 [[K:%.*]], 0
; CHECK-NEXT:    [[TMP2:%.*]] = getelementptr inbounds double, double* [[G:%.*]], i64 5
; CHECK-NEXT:    [[TMP3:%.*]] = load double, double* [[TMP2]], align 8
; CHECK-NEXT:    [[TMP4:%.*]] = fmul double [[TMP3]], 4.000000e+00
; CHECK-NEXT:    br i1 [[TMP1]], label [[TMP14:%.*]], label [[TMP5:%.*]]
; CHECK:         [[TMP6:%.*]] = getelementptr inbounds double, double* [[G]], i64 6
; CHECK-NEXT:    [[TMP7:%.*]] = load double, double* [[TMP6]], align 8
; CHECK-NEXT:    [[TMP8:%.*]] = fmul double [[TMP7]], 3.000000e+00
; CHECK-NEXT:    [[TMP9:%.*]] = insertelement <2 x double> undef, double [[TMP4]], i32 0
; CHECK-NEXT:    [[TMP10:%.*]] = insertelement <2 x double> [[TMP9]], double [[TMP8]], i32 1
; CHECK-NEXT:    [[TMP11:%.*]] = fadd <2 x double> [[TMP10]], <double 1.000000e+00, double 6.000000e+00>
; CHECK-NEXT:    [[TMP12:%.*]] = getelementptr inbounds double, double* [[G]], i64 1
; CHECK-NEXT:    [[TMP13:%.*]] = bitcast double* [[G]] to <2 x double>*
; CHECK-NEXT:    store <2 x double> [[TMP11]], <2 x double>* [[TMP13]], align 8
; CHECK-NEXT:    br label [[TMP24:%.*]]
; CHECK:         [[TMP15:%.*]] = getelementptr inbounds double, double* [[G]], i64 2
; CHECK-NEXT:    [[TMP16:%.*]] = getelementptr inbounds double, double* [[G]], i64 6
; CHECK-NEXT:    [[TMP17:%.*]] = load double, double* [[TMP16]], align 8
; CHECK-NEXT:    [[TMP18:%.*]] = fmul double [[TMP17]], 3.000000e+00
; CHECK-NEXT:    [[TMP19:%.*]] = insertelement <2 x double> undef, double [[TMP4]], i32 0
; CHECK-NEXT:    [[TMP20:%.*]] = insertelement <2 x double> [[TMP19]], double [[TMP18]], i32 1
; CHECK-NEXT:    [[TMP21:%.*]] = fadd <2 x double> [[TMP20]], <double 7.000000e+00, double 8.000000e+00>
; CHECK-NEXT:    [[TMP22:%.*]] = getelementptr inbounds double, double* [[G]], i64 3
; CHECK-NEXT:    [[TMP23:%.*]] = bitcast double* [[TMP15]] to <2 x double>*
; CHECK-NEXT:    store <2 x double> [[TMP21]], <2 x double>* [[TMP23]], align 8
; CHECK-NEXT:    br label [[TMP24]]
; CHECK:         ret i32 undef
;
  %1 = icmp eq i32 %k, 0
  %2 = getelementptr inbounds double, double* %G, i64 5
  %3 = load double, double* %2, align 8
  %4 = fmul double %3, 4.000000e+00
  br i1 %1, label %12, label %5

; <label>:5                                       ; preds = %0
  %6 = fadd double %4, 1.000000e+00
  store double %6, double* %G, align 8
  %7 = getelementptr inbounds double, double* %G, i64 6
  %8 = load double, double* %7, align 8
  %9 = fmul double %8, 3.000000e+00
  %10 = fadd double %9, 6.000000e+00
  %11 = getelementptr inbounds double, double* %G, i64 1
  store double %10, double* %11, align 8
  br label %20

; <label>:12                                      ; preds = %0
  %13 = fadd double %4, 7.000000e+00
  %14 = getelementptr inbounds double, double* %G, i64 2
  store double %13, double* %14, align 8
  %15 = getelementptr inbounds double, double* %G, i64 6
  %16 = load double, double* %15, align 8
  %17 = fmul double %16, 3.000000e+00
  %18 = fadd double %17, 8.000000e+00
  %19 = getelementptr inbounds double, double* %G, i64 3
  store double %18, double* %19, align 8
  br label %20

; <label>:20                                      ; preds = %12, %5
  ret i32 undef
}


;int foo(double *A, int n) {
;  A[0] = A[0] * 7.9 * n + 6.0;
;  A[1] = A[1] * 7.9 * n + 6.0;
;  A[2] = A[2] * 7.9 * n + 6.0;
;  A[3] = A[3] * 7.9 * n + 6.0;
;}

define i32 @foo4(double* nocapture %A, i32 %n) {
; CHECK-LABEL: @foo4(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[CONV:%.*]] = sitofp i32 [[N:%.*]] to double
; CHECK-NEXT:    [[ARRAYIDX3:%.*]] = getelementptr inbounds double, double* [[A:%.*]], i64 1
; CHECK-NEXT:    [[ARRAYIDX9:%.*]] = getelementptr inbounds double, double* [[A]], i64 2
; CHECK-NEXT:    [[ARRAYIDX15:%.*]] = getelementptr inbounds double, double* [[A]], i64 3
; CHECK-NEXT:    [[TMP0:%.*]] = bitcast double* [[A]] to <4 x double>*
; CHECK-NEXT:    [[TMP1:%.*]] = load <4 x double>, <4 x double>* [[TMP0]], align 8
; CHECK-NEXT:    [[TMP2:%.*]] = fmul <4 x double> [[TMP1]], <double 7.900000e+00, double 7.900000e+00, double 7.900000e+00, double 7.900000e+00>
; CHECK-NEXT:    [[TMP3:%.*]] = insertelement <4 x double> undef, double [[CONV]], i32 0
; CHECK-NEXT:    [[TMP4:%.*]] = insertelement <4 x double> [[TMP3]], double [[CONV]], i32 1
; CHECK-NEXT:    [[TMP5:%.*]] = insertelement <4 x double> [[TMP4]], double [[CONV]], i32 2
; CHECK-NEXT:    [[TMP6:%.*]] = insertelement <4 x double> [[TMP5]], double [[CONV]], i32 3
; CHECK-NEXT:    [[TMP7:%.*]] = fmul <4 x double> [[TMP6]], [[TMP2]]
; CHECK-NEXT:    [[TMP8:%.*]] = fadd <4 x double> [[TMP7]], <double 6.000000e+00, double 6.000000e+00, double 6.000000e+00, double 6.000000e+00>
; CHECK-NEXT:    [[TMP9:%.*]] = bitcast double* [[A]] to <4 x double>*
; CHECK-NEXT:    store <4 x double> [[TMP8]], <4 x double>* [[TMP9]], align 8
; CHECK-NEXT:    ret i32 undef
;
entry:
  %0 = load double, double* %A, align 8
  %mul = fmul double %0, 7.900000e+00
  %conv = sitofp i32 %n to double
  %mul1 = fmul double %conv, %mul
  %add = fadd double %mul1, 6.000000e+00
  store double %add, double* %A, align 8
  %arrayidx3 = getelementptr inbounds double, double* %A, i64 1
  %1 = load double, double* %arrayidx3, align 8
  %mul4 = fmul double %1, 7.900000e+00
  %mul6 = fmul double %conv, %mul4
  %add7 = fadd double %mul6, 6.000000e+00
  store double %add7, double* %arrayidx3, align 8
  %arrayidx9 = getelementptr inbounds double, double* %A, i64 2
  %2 = load double, double* %arrayidx9, align 8
  %mul10 = fmul double %2, 7.900000e+00
  %mul12 = fmul double %conv, %mul10
  %add13 = fadd double %mul12, 6.000000e+00
  store double %add13, double* %arrayidx9, align 8
  %arrayidx15 = getelementptr inbounds double, double* %A, i64 3
  %3 = load double, double* %arrayidx15, align 8
  %mul16 = fmul double %3, 7.900000e+00
  %mul18 = fmul double %conv, %mul16
  %add19 = fadd double %mul18, 6.000000e+00
  store double %add19, double* %arrayidx15, align 8
  ret i32 undef
}

;int partial_mrg(double *A, int n) {
;  A[0] = A[0] * n;
;  A[1] = A[1] * n;
;  if (n < 4) return 0;
;  A[2] = A[2] * n;
;  A[3] = A[3] * (n+4);
;}

define i32 @partial_mrg(double* nocapture %A, i32 %n) {
; CHECK-LABEL: @partial_mrg(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[CONV:%.*]] = sitofp i32 [[N:%.*]] to double
; CHECK-NEXT:    [[ARRAYIDX2:%.*]] = getelementptr inbounds double, double* [[A:%.*]], i64 1
; CHECK-NEXT:    [[TMP0:%.*]] = bitcast double* [[A]] to <2 x double>*
; CHECK-NEXT:    [[TMP1:%.*]] = load <2 x double>, <2 x double>* [[TMP0]], align 8
; CHECK-NEXT:    [[TMP2:%.*]] = insertelement <2 x double> undef, double [[CONV]], i32 0
; CHECK-NEXT:    [[TMP3:%.*]] = insertelement <2 x double> [[TMP2]], double [[CONV]], i32 1
; CHECK-NEXT:    [[TMP4:%.*]] = fmul <2 x double> [[TMP3]], [[TMP1]]
; CHECK-NEXT:    [[TMP5:%.*]] = bitcast double* [[A]] to <2 x double>*
; CHECK-NEXT:    store <2 x double> [[TMP4]], <2 x double>* [[TMP5]], align 8
; CHECK-NEXT:    [[CMP:%.*]] = icmp slt i32 [[N]], 4
; CHECK-NEXT:    br i1 [[CMP]], label [[RETURN:%.*]], label [[IF_END:%.*]]
; CHECK:       if.end:
; CHECK-NEXT:    [[ARRAYIDX7:%.*]] = getelementptr inbounds double, double* [[A]], i64 2
; CHECK-NEXT:    [[ARRAYIDX11:%.*]] = getelementptr inbounds double, double* [[A]], i64 3
; CHECK-NEXT:    [[TMP6:%.*]] = bitcast double* [[ARRAYIDX7]] to <2 x double>*
; CHECK-NEXT:    [[TMP7:%.*]] = load <2 x double>, <2 x double>* [[TMP6]], align 8
; CHECK-NEXT:    [[ADD:%.*]] = add nsw i32 [[N]], 4
; CHECK-NEXT:    [[CONV12:%.*]] = sitofp i32 [[ADD]] to double
; CHECK-NEXT:    [[TMP8:%.*]] = insertelement <2 x double> [[TMP2]], double [[CONV12]], i32 1
; CHECK-NEXT:    [[TMP9:%.*]] = fmul <2 x double> [[TMP8]], [[TMP7]]
; CHECK-NEXT:    [[TMP10:%.*]] = bitcast double* [[ARRAYIDX7]] to <2 x double>*
; CHECK-NEXT:    store <2 x double> [[TMP9]], <2 x double>* [[TMP10]], align 8
; CHECK-NEXT:    br label [[RETURN]]
; CHECK:       return:
; CHECK-NEXT:    ret i32 0
;
entry:
  %0 = load double, double* %A, align 8
  %conv = sitofp i32 %n to double
  %mul = fmul double %conv, %0
  store double %mul, double* %A, align 8
  %arrayidx2 = getelementptr inbounds double, double* %A, i64 1
  %1 = load double, double* %arrayidx2, align 8
  %mul4 = fmul double %conv, %1
  store double %mul4, double* %arrayidx2, align 8
  %cmp = icmp slt i32 %n, 4
  br i1 %cmp, label %return, label %if.end

if.end:                                           ; preds = %entry
  %arrayidx7 = getelementptr inbounds double, double* %A, i64 2
  %2 = load double, double* %arrayidx7, align 8
  %mul9 = fmul double %conv, %2
  store double %mul9, double* %arrayidx7, align 8
  %arrayidx11 = getelementptr inbounds double, double* %A, i64 3
  %3 = load double, double* %arrayidx11, align 8
  %add = add nsw i32 %n, 4
  %conv12 = sitofp i32 %add to double
  %mul13 = fmul double %conv12, %3
  store double %mul13, double* %arrayidx11, align 8
  br label %return

return:                                           ; preds = %entry, %if.end
  ret i32 0
}

%class.B.53.55 = type { %class.A.52.54, double }
%class.A.52.54 = type { double, double, double }

@a = external global double, align 8

define void @PR19646(%class.B.53.55* %this) {
; CHECK-LABEL: @PR19646(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    br i1 undef, label [[IF_END13:%.*]], label [[IF_END13]]
; CHECK:       sw.epilog7:
; CHECK-NEXT:    [[DOTIN:%.*]] = getelementptr inbounds [[CLASS_B_53_55:%.*]], %class.B.53.55* [[THIS:%.*]], i64 0, i32 0, i32 1
; CHECK-NEXT:    [[TMP0:%.*]] = load double, double* [[DOTIN]], align 8
; CHECK-NEXT:    [[ADD:%.*]] = fadd double undef, 0.000000e+00
; CHECK-NEXT:    [[ADD6:%.*]] = fadd double [[ADD]], [[TMP0]]
; CHECK-NEXT:    [[TMP1:%.*]] = load double, double* @a, align 8
; CHECK-NEXT:    [[ADD8:%.*]] = fadd double [[TMP1]], 0.000000e+00
; CHECK-NEXT:    [[_DY:%.*]] = getelementptr inbounds [[CLASS_B_53_55]], %class.B.53.55* [[THIS]], i64 0, i32 0, i32 2
; CHECK-NEXT:    [[TMP2:%.*]] = load double, double* [[_DY]], align 8
; CHECK-NEXT:    [[ADD10:%.*]] = fadd double [[ADD8]], [[TMP2]]
; CHECK-NEXT:    br i1 undef, label [[IF_THEN12:%.*]], label [[IF_END13]]
; CHECK:       if.then12:
; CHECK-NEXT:    [[TMP3:%.*]] = load double, double* undef, align 8
; CHECK-NEXT:    br label [[IF_END13]]
; CHECK:       if.end13:
; CHECK-NEXT:    [[X_1:%.*]] = phi double [ 0.000000e+00, [[IF_THEN12]] ], [ [[ADD6]], [[SW_EPILOG7:%.*]] ], [ undef, [[ENTRY:%.*]] ], [ undef, [[ENTRY]] ]
; CHECK-NEXT:    [[B_0:%.*]] = phi double [ [[TMP3]], [[IF_THEN12]] ], [ [[ADD10]], [[SW_EPILOG7]] ], [ undef, [[ENTRY]] ], [ undef, [[ENTRY]] ]
; CHECK-NEXT:    unreachable
;
entry:
  br i1 undef, label %if.end13, label %if.end13

sw.epilog7:                                       ; No predecessors!
  %.in = getelementptr inbounds %class.B.53.55, %class.B.53.55* %this, i64 0, i32 0, i32 1
  %0 = load double, double* %.in, align 8
  %add = fadd double undef, 0.000000e+00
  %add6 = fadd double %add, %0
  %1 = load double, double* @a, align 8
  %add8 = fadd double %1, 0.000000e+00
  %_dy = getelementptr inbounds %class.B.53.55, %class.B.53.55* %this, i64 0, i32 0, i32 2
  %2 = load double, double* %_dy, align 8
  %add10 = fadd double %add8, %2
  br i1 undef, label %if.then12, label %if.end13

if.then12:                                        ; preds = %sw.epilog7
  %3 = load double, double* undef, align 8
  br label %if.end13

if.end13:                                         ; preds = %if.then12, %sw.epilog7, %entry
  %x.1 = phi double [ 0.000000e+00, %if.then12 ], [ %add6, %sw.epilog7 ], [ undef, %entry ], [ undef, %entry ]
  %b.0 = phi double [ %3, %if.then12 ], [ %add10, %sw.epilog7 ], [ undef, %entry], [ undef, %entry ]
  unreachable
}