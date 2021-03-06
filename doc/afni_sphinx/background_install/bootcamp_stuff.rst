
.. _Bootcamping:

***********************
**Basic Bootcamp Prep**
***********************

.. note:: This is a very preliminary guide for preparing for a
          Bootcamp.  But it is accurate!

#. **Get AFNI on your computer**

   * **IF** you do *not* have AFNI set up on your computer system, then
     please see the step-by-step instructions:

     - for :ref:`Linux <install_steps_linux>`
       
     - for :ref:`Mac OS <install_steps_mac>`

     - for Windows users... eek.

     Methods for checking/evaluating each setup are also described on
     those pages.  PLEASE make sure you have verified that all is well
     with AFNI on your computer.

   * **ELSE** (you have AFNI set up on your computer, but you are
     not certain that it is the most up-to-date version), please
     do the following:

     + *if you have installed pre-compiled binaries on your computer (the
       most common approach)*::
   
         @update.binaries.afni -d

     + *if you compile AFNI from source*, then go :ref:`here
       <download_SRC>` and download the latest source.


   .. _install_bootcamp:

#. **Install AFNI Bootcamp class data.**

   This step is required if you are about to attend a :ref:`Bootcamp
   <Bootcamping>`::

      curl -O https://afni.nimh.nih.gov/pub/dist/edu/data/CD.tgz
      tar xvzf CD.tgz
      cd CD
      tcsh s2.cp.files . ~
      cd ..
      
   In order, these commands: get the tarred+zipped directory that
   contains the class data (and is hence named "CD"), downloading it
   to the current location in the terminal; untars/unzips it (=opens
   it up); goes into the newly opened directory; executes a script to
   copy the files to ``$HOME/CD/``; and finally exits the directory.

   At this point, if there have been no errors, you can delete/remove
   the tarred/zipped package, using "``rm CD.tgz``".  If you are
   *really* confident, you can also deleted the CD tree in the present
   location (but leaving it in ``$HOME/CD/``).


#. **Verify the setup.**

   Please use ``afni_system_check.py`` to verify the installation
   (of AFNI binaries, libraries and class data). ::

      afni_system_check.py -check_all

   If there are any questions about your setup, you will be asked
   to send the output of that command via email.  To do so, please
   run the same command, but save the output to a text file called
   ``output.afni.sys.check.txt``. ::

      afni_system_check.py -check_all > output.afni.sys.check.txt

   That file can then be attached to an email message.


|

|

