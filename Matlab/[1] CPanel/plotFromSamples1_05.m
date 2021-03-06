% This scipt loads and plots data from a .mat file
clc
clear all
gxFdata = 0;
gyFdata = 0;
gzFdata = 0;


% Loads samples from file
dat = load('AccSamples.mat');

% Plots raw data
figure(1);
plot(dat.time,dat.x);
title('Noisy acceleration along X axis');
xlabel('Time [s]');
ylabel('Amplitude [m*s^-2]');
grid on
grid minor

% % Plots raw data
% figure(2);
% plot(dat.x);
% title('Noisy acceleration along X axis');
% xlabel('Samples [s]');
% ylabel('Amplitude [m*s^-2]');
% grid on
% grid minor

% Plots magnitude spectrum of the signal
X_mags=abs(fft(dat.x));
figure(3)
plot(X_mags);
xlabel('DFT Bins');
ylabel('Magnitude');

% Plots first half of DFT (normalized frequency)
num_bins = length(X_mags);
plot([0:1/(num_bins/2 -1):1],X_mags(1:num_bins/2))
xlabel('Normalized frequency [\pi rads/samples]');
ylabel('Magnitude');

% Designs a second order filter using a butterworth design guidelines
<<<<<<< HEAD
[b a] = butter(2,0.05,'low');
=======

[b a] = butter(2,0.10,'low');
[b a] = butter(2,0.1,'low');
>>>>>>> origin/master

% Plot the frequency response (normalized frequency)
H = freqz(b,a,floor(num_bins/2));
hold on
plot([0:1/(num_bins/2 -1):1], abs(H), 'r');

% Filters the signal using coefficients obtained by the butter filter
% design
x_filtered = filter(b,a,dat.x);

% Plots the filtered signal
figure(4)
plot(x_filtered,'r')
title('Filtered Signal - Second Order Butterworth');
xlabel('Samples');
ylabel('Amplitude');

% Redesign the filter using a higher order filter
[b2 a2] = butter(3,0.2,'low');

% Plots the magnitude spectrum and compare with lower order
H2 = freqz(b2,a2,floor(num_bins/2));
figure(3);
hold on
plot([0:1/(num_bins/2 -1):1], abs(H2), 'r');

%filter the noisy signal and plots the result
x_filtered2 = filter(b2,a2,dat.x);
figure(5)
plot(x_filtered2,'g');
title('Filtered Signal - 20 Order Butterworth');
xlabel('Samples');
ylabel('Amplitude');

% Comparison between filters: first order low pass Vs second order
alpha = 0.05;
axF = 0;
axF2 = 0;
axF2m1 = 0;
gxFdata = zeros(num_bins,1);
gxFdata2 = zeros(num_bins,1);

for i = 1:num_bins
    axF = (1 - alpha)*axF + alpha*dat.x(i);
    if (i==1)
        axF2 = b(1)*dat.x(i);
    end
    if (i==2)
        axF2 = b(1)*dat.x(i) +b(2)*dat.x(i-1)...
            - a(2)*gxFdata2(num_bins)
    end
    if (i>2 && i<num_bins-2)
        axF2 = b(1)*dat.x(i)+b(2)*dat.x(i-1)...
            + b(3)*dat.x(i-2)...
            - a(2)*gxFdata2(num_bins) ...
            - a(3)*gxFdata2(num_bins-1); 
    end
    gxFdata = [ gxFdata(2:end) ; axF ];
    gxFdata2 = [ gxFdata2(2:end) ; axF2 ];
end

figure(5)
plot(dat.x,'r');
hold on
plot(gxFdata,'g');
hold on
plot(gxFdata2,'.');
hold on
plot(x_filtered,'b');
hold on
plot(x_filtered2,'r-');
title('Filter Comparison');
xlabel('Samples [s]');
ylabel('Amplitude [m*s^-2]');
grid on
grid minor

