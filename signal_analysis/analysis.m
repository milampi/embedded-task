pkg load signal
myfft_tools
graphics_toolkit('fltk')

% Load the 100 ms data
load("out1.tsv")
load("out2.tsv")
load("out3.tsv")

% Remove offsets from timestamps and divide by 1000 to scale to seconds
out1(:,1) = (out1(:,1) .- out1(1,1))./1000;
out2(:,1) = (out2(:,1) .- out2(1,1))./1000;
out3(:,1) = (out3(:,1) .- out3(1,1))./1000;

LineWidth=0.5;

% out1
hf = figure('color', [0.7, 0.7, 0.7]);
set(gcf,'InvertHardCopy','off')
subplot(1,2,1)
plot(out1(1:100,1), out1(1:100,2), "Linewidth", LineWidth)
set(gca,'color',[0.5 0.5 0.5])
title ("Channel 1 signal");
xlabel ("time (s)");
ylabel ("Amplitude");
annotation("textarrow", [0.37 0.27], [0.65 0.74], "string", "Glitch", "headlength", 2, "headwidth", 2 )

[x_fft y_fft] = myfft( out1(:,2), max(out1(:,1)) );
[pks idx] = findpeaks(y_fft,"MinPeakDistance", 100); x_fft( idx )

subplot(1,2,2)
plot(x_fft, y_fft, "Linewidth", LineWidth, x_fft(idx), y_fft(idx), "Linewidth", LineWidth, "xm")
set(gca,'color',[0.5 0.5 0.5])
title ("Channel 1 frequency");
xlabel ("Frequency (Hz)");
ylabel ("Amplitude");
print (hf, "channel1", "-dpng" )

input("Press enter");

% out2
hf = figure('color', [0.7, 0.7, 0.7]);
set(gcf,'InvertHardCopy','off')
subplot(1,2,1)
plot(out2(1:162,1), out2(1:162,2), "Linewidth", LineWidth)
set(gca,'color',[0.5 0.5 0.5])
title ("Channel 2 signal");
xlabel ("time (s)");
ylabel ("Amplitude");
annotation("textarrow", [0.35 0.245], [0.65 0.25], "string", "Glitch", "headlength", 2, "headwidth", 2 )

[x_fft y_fft] = myfft( out2(:,2), max(out2(:,1)) );
[pks idx] = findpeaks(y_fft,"MinPeakDistance", 100); x_fft( idx )

subplot(1,2,2)
plot(x_fft, y_fft, "Linewidth", LineWidth, x_fft(idx), y_fft(idx), "Linewidth", LineWidth, "xm")
set(gca,'color',[0.5 0.5 0.5])
title ("Channel 2 frequency");
xlabel ("Frequency (Hz)");
ylabel ("Amplitude");
print (hf, "channel2", "-dpng" )

input("Press enter");

% out3
hf = figure('color', [0.7, 0.7, 0.7]);
set(gcf,'InvertHardCopy','off')
subplot(1,2,1)
bar(out3(1:280,1), out3(1:280,2), "histc", "facecolor", [0.0, 0.0, 0.5])
set(gca,'color',[0.5 0.5 0.5])
title ("Channel 3 signal");
xlabel ("time (s)");
ylabel ("Amplitude");
set(gca,'color',[0.5 0.5 0.5])
ylim([0 5.5])

% Calculate peak widths
[pks idx1] = findpeaks( out3(:,2), "MinPeakDistance", 1 );
neg=out3;
neg(:,2)=(-1.*neg(:,2)).+5;
[pks idx2] = findpeaks( neg(:,2), "MinPeakDistance", 1 );
widths = neg(idx2,1) .- out3(idx1(1:end-1),1);

subplot(1,2,2)
hist( widths, unique(widths), "facecolor", [0.0, 0.5, 0.0] )
title ("Channel 3 pulse widths histogram");
xlabel ("width (s)");
ylabel ("Count");
set(gca,'color',[0.5 0.5 0.5])
print (hf, "channel3", "-dpng" )



%plot(out3(1:280,1), out3(1:280,2), "Linewidth", LineWidth, out3(idx1,1), out3(idx1,2), "Linewidth", LineWidth, "xm", neg(idx2,1), neg(idx2,2), "Linewidth", LineWidth, "+m" )
%ylim([0 5.5])


