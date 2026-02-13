# GC-MOX Dual Sensor System

<p align="center">
<img width="359" height="255" alt="Screen Shot 2026-02-13 at 1 27 36 PM" src="https://github.com/user-attachments/assets/0583f9c6-6d1c-4b81-82c1-000f88b9f900" />
</p>



Raw Gas Resistance Mode (Forced Mode, Single Temperature)

A dual Bosch BME690 MOX gas sensor system running in forced mode at a constant heater temperature, designed for:

Raw VOC response monitoring

Transit-time estimation (GC experiments)

Drift observation

Dual-sensor comparison

Currently operating without FFT / spectral processing.

# Hardware

**Components**

- Raspberry Pi 5

- Bosch BME690 sensors

- I²C interface

- GC Chamber / Multi Capillary GC

- Mini Pump: 12V DC

- Jumper Cables (≥15)

- Breadboard

- Buck Converter

- Clear vinyl tubing
  - 3mm ideal
  - 5mm acceptable

**Sensor Addresses**

Sensor Address	Typical Roles:
Sensor 1	0x77	Reference / near injector
Sensor 2	0x76	Delayed / after GC column

**Notes**

Sensors share the same I²C bus

Change address of 2nd sensor by scratching away the metal section

Ensure stable 3.3V supply for BME690

Pump requires a buck converter to step the voltage down to 5V from the incoming supply

# Physical Connections

**I2C**
Connect Both Sensors in Parrelel using the breadboard, to the Raspberry Pi. This will ensure that both addresses are on the same I2C Bus.
<p align="center">
<img width="527" height="323" alt="Screen Shot 2026-02-13 at 12 16 11 PM" src="https://github.com/user-attachments/assets/3fbfa658-3d96-448a-a6d8-9a4fb973899a" />
</p>

To check this use i2c detect in the raspberry pi terminal.

<p align="center">
<img width="412" height="251" alt="Screen Shot 2026-02-13 at 11 42 11 AM" src="https://github.com/user-attachments/assets/4dda0601-e6d5-4102-92dd-7fa281d5905f" />
</p>

If you see both 76 and 77 are connected then you are good to start using the programs.

# Physical Layout
<p align="center">
<img width="356" height="480" alt="Screen Shot 2026-02-13 at 11 20 14 AM" src="https://github.com/user-attachments/assets/0d94ad6f-c842-431e-8c7f-0ddf3105487d" />
</p>

Connect the pipes starting from the pump and then splitting betwen the 1st sensor and into the GC Column. The rest of the pipe should go from the end of the GC column to the 2nd sensor.

# Collecting Data

First navigate to the correct folder:
   - cd GC_MOX
   - cd forced_mode
Next build forced_2_gas.c:
   - make clean
   - make
Now you can collect data:
   - Connect the power source to the pump
   - Change the name of OUT_CSV to what you want to name the CSV file as.
   - Wait for the figures to show both the input and output as steady (minimal change in gas resistance)
   - Inject 1ml samples of the gas into the nosel of the pump.
   - Wait for input and output to settle to a steady state before reinjecting the sample
   - Finally when all data is collected. Move your CSV file into the data file under forced_mode. This will allow it to run in the notebook      later.

# Open the notebook:

Open the correct folder:
 - From within forced_mode run:   _cd data_

   
Open the virtual environment:
_source notebook_env/bin/activate_

Install the relevent libraries if not preinstalled:
 - jupyter
 - pandas
 - numpy
 - csv
 - seaborn

- _pip3 install jupyter pandas numpy csv seaborn_

Launch Jupyter:
 - jupyter notebook
 - This will open the data folder in jupyter
 - Open acetone_isopropyl.ipynb

# Interprating Data

The notebook has data sets preloaded. If you wish to load your own data from scratch delete these. Keep cell 1 to cell 5 as this includes importing libraries, setting global constants and the methods. However it might be worth having a look at the existing data first to understand how the methods are used.

**Using the methods**

1. Use _load_csv_ to add your new data set to the program. Input the filename and the function will load the csv file and from it return arrays for output times, input times as well as gas resistance inputs and outputs.  

2. Next use the _plot_gc_ function to plot the entirety of your dataset. Input the 4 arrays and the function will show you all the peaks of your data.

3. Use the _plot_gc_ function again, however this time input the arrays followed by two integers and a string for your plot title. (7 input variables)
    - The two integers will set a lower and upper time limit for your data. These values should isolate one of your peaks for further evaluation.

4. Use _get_peak_width_ to get both the width in time of the peak, as well as the time between input and output peaks. Input into the function a lower limit for your input peak, an upper limit for your input peak. As well as the same for your output peak. Finally input a string for the type of smell you are measuring. This will ensure the method adds the data to the appropriate arrays. If you wish to analyze a new smell not listed. Add a new global times and width array, where the other are and edit the method to look for the string of this smell

5. Repeat Steps 2 to 4 to collect the data from the rest of your peaks. If you wish to add another csv file for the same smell to get more peaks repeat Step 1 as well.

6. Now run the distribution method. Input a times or width array for any smell and it will give you the mean, standard deviation as well as plot the probability distribution.

7. You can complete these steps for multiple different smells, and compare the datasets.
   - A good block of code for running this data is:  




