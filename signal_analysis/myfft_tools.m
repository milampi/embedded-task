pkg load signal

% Calculates FFT for a given burst of measurements
% Example of use:
% P = 2.0; % Legth of sample in seconds
% x = linspace(0,P,800) ; % Times in seconds when each sample was measured
% y = sin(50 * 2*pi * x) + 1/2 * sin(80 * 2*pi * x); % Simulation of measured samples
% [x_fft y_fft] = myfft(y, P);

% Example of analysis
% [pks idx] = findpeaks(y_fft);
% x_fft( idx )
% plot(x_fft, y_fft, x_fft(idx), y_fft(idx), "xm")


function [x_frq yf] = myfft(y, length_in_time)
	N = length(y);
	P = length_in_time;
	T = P / N; % Width of a single sample in data
	SF = 1/T;  % Sampling frequency

	% x-axel for frequency data
	% We dropt the last sample because octave uses 1 based indexing
	% We halve the x-axel dataset to match the FFT output that discards the mirror part
	x_frq = linspace(0,SF,N+1)(1:end-1)(1:N/2);

	% Calculate FFT
	yf = abs(fft(y));
	yf = yf(1:N/2);

endfunction

