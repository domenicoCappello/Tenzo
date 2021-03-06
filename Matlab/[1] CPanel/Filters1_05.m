function Filter()
clear all;
clc;

global InputBufferSize;
global arduinoAdd;
global matlabAdd;
global time;
global axFilt;
global ayFilt;
global azFilt;
global buf_len;
global index;
global alphaAcc;
global xbee;
global finished;

arduinoAdd = 1;
matlabAdd = 2;
% 20 cmd: 184+1
InputBufferSize = 354;

version = 1;
finished = 0;
buf_len = 40;
index = 1:buf_len;
alphaAcc = 1;
axFilt = 0;
ayFilt = 0;
azFilt = 0;
mess = 0;

axData = zeros(buf_len,1);
ayData = zeros(buf_len,1);
azData = zeros(buf_len,1);
axFiltData = zeros(buf_len,1);
ayFiltData = zeros(buf_len,1);
azFiltData = zeros(buf_len,1);

time = zeros(buf_len,1);

disp('Arduino Connect ');
baudrate = 9600;    
cont = 0;
count =0;
delete(instrfindall);

%% Setting up serial communication

% (instrfind) whos 'Port' property is set to 'COM3'

oldSerial = instrfind('Port', 'COM3'); 
% can also use instrfind() with no input arguments to find 
% ALL existing serial objects

% if the set of such objects is not(~) empty
if (~isempty(oldSerial))  
    disp('WARNING:  port in use.  Closing.')
    delete(oldSerial)
end
% XBee expects the end of commands to be delineated by a carriage return.

xbee = serial('COM4','baudrate',baudrate,'terminator','CR');

% Max wait time
set(xbee, 'TimeOut', 10);  
% One message long buffer
set(xbee, 'InputBufferSize',InputBufferSize);
% Open the serial
fopen(xbee);    

timerXbee = timer('ExecutionMode','FixedRate','Period',0.1,'TimerFcn',{@storeDataFromSerial});

%% Creates window
handles.hFig = figure('Menubar','none');
AccData = warning('off', 'MATLAB:uitabgroup:OldVersion');

%# Home UI components
uicontrol('Style','text', 'String','Data Acquisition', ...
    'Position', [55 310 420 50],...
    'FontSize',20,'FontWeight','bold');

uicontrol('Style','text', 'String',version, ...
    'Position', [55 268 420 50],...
    'FontSize',10,'FontWeight','normal');

handles.start = uicontrol('Style','pushbutton', 'String','Record', ...
    'Position', [70 50 120 30],...
    'Callback',@startCallback);

handles.stop = uicontrol('Style','pushbutton', 'String','Stop', ...
    'Position', [350 50 120 30],...
    'Callback',@stopCallback);

    function startCallback(src,eventData)        
        start(timerXbee);  
        pause();
        fwrite(xbee,'A');
    end

    function stopCallback(src,eventData)
        disp('Saving samples');
        cont = 0;
        plotCrist();
    end
        
    
      
    %% Serial routine
    
    function storeDataFromSerial(obj,event,handles)
        while (get(xbee, 'BytesAvailable')~=0)
            [mess,count] = fread(xbee);
            disp('Reading incoming buffer. Dimensions:');
            % Debug stuf
            disp(count);
            disp(mess);
            if (count == (InputBufferSize))% && mess(2) == matlabAdd && mess(1) == arduinoAdd)
                %versionArd = typecast([uint8(mess(3)), uint8(mess(4)),uint8(mess(5)), uint8(mess(6))], 'int32');
                numCmd = mess(7+2);
                %Arduino  tells the receiver where commands start
                readFrom = mess(8+2);        
                sizeOfEachCmd = mess(9+2);
                totMessLength = typecast([uint8(mess(10)), uint8(mess(11))], 'int16');
                disp('Total message length (header value):');
                disp(totMessLength);

                %% Read Commands
                for i = 0:(numCmd-1)
                    typei = (readFrom+1)+i*sizeOfEachCmd +2
                    type = mess(typei);
                    %disp('Message #:');
                    %disp(i+1);
                    %disp('Connection channel');
                    if (type == 32)
                        cont=cont+1
                        % Pid Roll AGG
                        x = typecast([uint8(mess(typei + 1)), uint8(mess(typei + 2)),uint8(mess(typei + 3)), uint8(mess(typei + 4))], 'single');                        
                        y = typecast([uint8(mess(typei + 5)), uint8(mess(typei + 6)),uint8(mess(typei + 7)), uint8(mess(typei + 8))], 'single');                        
                        z = typecast([uint8(mess(typei + 9)), uint8(mess(typei + 10)),uint8(mess(typei + 11)), uint8(mess(typei + 12))], 'single');                        
                        t = typecast([uint8(mess(typei + 13)), uint8(mess(typei + 14)),uint8(mess(typei + 15)), uint8(mess(typei + 16))], 'single');
                                                
                        %Filt Data
                        axFilt = (1 - alphaAcc)*axFilt + alphaAcc*x;
                        ayFilt = (1 - alphaAcc)*ayFilt + alphaAcc*y;
                        azFilt = (1 - alphaAcc)*azFilt + alphaAcc*z;
                        
                        % Raw Data
                        axData = [ axData(2:end) ; x ];
                        ayData = [ ayData(2:end) ; y ];
                        azData = [ azData(2:end) ; z ]; 
                        
                        axFiltData = [ axFiltData(2:end) ; axFilt ];
                        ayFiltData = [ ayFiltData(2:end) ; ayFilt ];
                        azFiltData = [ azFiltData(2:end) ; azFilt ]; 
                        %Plot
                        
                        time = [ time(2:end) ; t ];
                    end
                end
            end
        end %while
        if cont >= 20
            fwrite(xbee,'J');
            stop(timerXbee);
            finished = 1;
        end
    end

    function plotCrist(src,eventData)
        if (finished == 1)
            AccData = matfile('dataAcc.mat','Writable',true);
            AccData.x = axData;
            AccData.y = ayData;
            AccData.z = azData;
            AccData.t = time;
            disp('Records saved in dataAcc.mat file. Check that out!');
            figure;
            %Plot the X acc
            h1 = subplot(3,1,1);
            %set(h1,'title','X acc (ms^-2)');
            plot(h1,time,axData,'b','LineWidth',1);%,'MarkerEdgeColor','k','MarkerFaceColor','g','MarkerSize',5);
            grid on
            %grid minor
            %xlim([0 100]);
            %hold on;
            %plot(h1,time,axFiltData,'r','LineWidth',1);
            %hold off;
            %xlabel('Time');
            %ylabel('Wx');
            axis([time(1) time(end) -1 1]);
            %hold on;
            h2 = subplot(3,1,2);
            %title('Y angular velocity in deg/s');
            plot(h2,time,ayData,'b','LineWidth',1);%,'MarkerEdgeColor','k','MarkerFaceColor','g','MarkerSize',5);
            grid on
            %grid minor
            %hold on;
            %plot(h1,time,ayFiltData,'r','LineWidth',1);
            %hold off;
            %xlabel('Time');
            %ylabel('Wy Acc');
            %axis([1 buf_len -80 80]);
            h3 = subplot(3,1,3);
            %title('Z angular velocity in deg/s');
            %hold on;
            plot(h3,time,azData,'b','LineWidth',1);%,'MarkerEdgeColor','k','MarkerFaceColor','g','MarkerSize',5);
            grid on
            %grid minor
            %hold on;
            %plot(h3,time,azFiltData,'g','LineWidth',2);
            %hold off;
            %axis([1 buf_len -80 80]);
            %xlabel('Time');
            %ylabel('Wz Acc');
        end
    end
end